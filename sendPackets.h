#ifndef USB_SEND_SENDPACKETS_H
#define USB_SEND_SENDPACKETS_H

#include <array>
#include <cstring>
#include <iostream>

#include <unistd.h>

#include <libusb-1.0/libusb.h>

using Byte = unsigned char;

enum PayloadKinds : Byte {
  // Some other kind of meta-packet that surrounds a transaction
  // Might be some context thing, like pushd/popd in shell
  BEGIN_END_PAYLOAD = 0xf5,
  // Is sent for light data packets
  LIGHT_DATA = 0xf3,
  // A kind of reset message
  RESET_CONTROL = 0xf1
};

// LIGHT_DATA subtype values when byte3 is 0x04
constexpr Byte lightDataSubtypes[] = {0x46, 0x49, 0x4f, 0x51, 0x57, 0x59, 0x5f, 0x61, 0x67, 0x69, 0x6f};
// LIGHT_DATA subtype values when byte3 is 0x00
constexpr Byte lightDataSubtypes2[] = {0x32, 0x38, 0x42 /* ??? */};
// LIGHT_DATAs subtype when byte4 is 0x02
constexpr Byte profileSubType = 0x2c;

constexpr Byte beginSubtype = 0x00;
constexpr Byte endSubtype = 0x01;

enum ResetSubtypes : Byte {
  // Looks like it resets the mouse like libusb_reset_device
  RESET_DEVICE = 0x00,
  // Gives broken pipe
  BROKEN_PIPE = 0x01,
  // Appears to tell the mouse to refresh the current light animation state
  // Works with all the numbers from 0x02 to 0xff
  // Might be some sort of ACK, while using byte3 to specify what to acknowledge
  REFRESH_LIGHT_STATE = 0x02
};

enum PatternTypes : Byte { NONE = 0x00, TRACE = 0x06, REACTIVE = 0x07, WAVE = 0x02, SECONDARY_PATTERN = 0x01 };

enum SecondaryPatternTypes : Byte { FLASH = 0x10, SOLID_COLOR = 0x02, RAINBOW = 0x08, BREATHING = 0x04 };

struct RGBColor {
  Byte red = 0x00;
  Byte green = 0x00;
  Byte blue = 0x00;
};

constexpr RGBColor RED = {0xff, 0x00, 0x00};
constexpr RGBColor GREEN = {0x00, 0xff, 0x00};
constexpr RGBColor BLUE = {0x00, 0x00, 0xff};
constexpr RGBColor WHITE = {0xff, 0xff, 0xff};
constexpr RGBColor BLACK = {0x00, 0x00, 0x00};

enum Intensity : Byte { OFF = 0x00, LOW = 0x01, MEDIUM = 0x02, HIGH = 0x03 };

enum Speed : Byte { STOP = 0x00, SLOW = 0x08, NORMAL = 0x05, FAST = 0x02, FAST_FLASH_ALT = 0x01 };

struct PacketPayload {
  // Appears to always be 0x02
  // Possibly report count? Maybe current mouse profile?
  // Found one 128-byte package with 0x03 here
  Byte byte0 = 0x02;
  // What type of message this packet is
  PayloadKinds kind = LIGHT_DATA;
  // Extra data about current payload, depends on kind
  // LIGHT_DATA: from lightDataSubtypes
  // RESET_CONTROL: from ResetSubtypes
  // BEGIN_END_PAYLOAD: looks like it might be 0 for start and 1 for end
  Byte subType = lightDataSubtypes[0];
  // Unknown byte
  // LIGHT_DATA: 0x04, 0x01, 0x02, 0x03
  // RESET_CONTROL: seen as 0x04, 0x02, 0x10, 0x01, 0x08
  // BEGIN_END_PAYLOAD: seems to always be 0x00
  Byte byte3 = 0x04;

  // The rest of the bytes seem be 0 for non-LIGHT_DATA packets

  // Unknown byte
  // 0x02, 0x01, 0x06, 0x00, 0x04
  // Might be a tag specifying what the following bytes represent
  // Light data doesn't work if it isn't 0x06, and intensity packets don't work
  // if it isn't 0x01
  Byte byte4 = 0x00;
  // Unknown bytes
  // Might be RGB color, were never seen to differ from 0
  // Might also be padding; these would align the above to 8 bytes
  Byte bytes567[3] = {0x00, 0x00, 0x00};
  // Specifies light color, intensity or profile
  union {
    RGBColor color = {0x00, 0x00, 0x00};
    struct {
      union {
        Intensity intensity;
        Byte profile;
      };
      Byte : 8;
      Byte : 8;
    };
  };
  // Light pattern
  PatternTypes pattern = NONE;
  // Pattern speed
  Byte speed = STOP;
  // Only specified if pattern is SECONDARY_PATTERN
  SecondaryPatternTypes realPattern;
  // Unknown bytes
  // Never seen differ from zero; likely to just be struct padding
  Byte byte14 = 0x00;
  Byte byte15 = 0x00;
};

class PacketSender {
  libusb_device_handle* handle;

 public:
  explicit PacketSender(const DeviceHandle& handle) : handle(handle.getHandle()) {}

  template <size_t DATA_SIZE = 16>
  void sendControlTransfer(Byte data[]) {
    int res = libusb_control_transfer(
        handle,
        0x21,    // bmRequestType (host to device)
        0x09,    // bRequest (set configuration)
        0x0302,  // wValue (configuration value)
        0x0002,  // wIndex (specifies the target interface; wireshark captures the packet on interface 0, but this
                 // control
        // transfer is apparently sent on interface 2, and both must be claimed for the transfer to work)
        data,       // Payload
        DATA_SIZE,  // wLength, payload size in bytes
        0           // unlimited timeout
    );
    if (res < 0) {
      std::cerr << "transfer failed: " << libusb_error_name(res) << std::endl;
      std::cerr << "errno: " << strerror(errno) << std::endl;
      std::cerr << "tried to write " << DATA_SIZE << std::endl;
    }
  }

  template <size_t DATA_SIZE = 16>
  void sendControlTransfer(std::array<unsigned char, DATA_SIZE> data) {
    sendControlTransfer<DATA_SIZE>(data.data());
  }

  void sendControlTransfer(PacketPayload payload) {
    static_assert(sizeof(payload) == 16);
    sendControlTransfer(reinterpret_cast<Byte*>(&payload));
  }

  void setIntensity(Intensity intensity) {
    sendControlTransfer(
        PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[4], .byte4 = 0x01, .intensity = intensity});
  }

  void refresh() { sendControlTransfer(PacketPayload{.kind = RESET_CONTROL, .subType = REFRESH_LIGHT_STATE}); }

  void setPattern(PatternTypes pattern) {
    sendControlTransfer(
        PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[3], .byte4 = 0x06, .pattern = pattern});
  }

  void setSecondaryPattern(SecondaryPatternTypes pattern, RGBColor color, Speed speed) {
    sendControlTransfer(PacketPayload{.kind = LIGHT_DATA,
                                      .subType = lightDataSubtypes[3],
                                      .byte4 = 0x06,
                                      .color = color,
                                      .pattern = SECONDARY_PATTERN,
                                      .speed = speed,
                                      .realPattern = pattern});
  }

  void setProfile(Byte profile) {
    // FIXME
    sendControlTransfer(
        PacketPayload{.kind = LIGHT_DATA, .subType = profileSubType, .byte3 = 0x00, .byte4 = 0x02, .profile = profile});
    refresh();
  }

  void turnOff() {
    sendControlTransfer(PacketPayload{.kind = BEGIN_END_PAYLOAD, .subType = beginSubtype});
    sendControlTransfer(PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[0], .byte4 = 0x02});
    sendControlTransfer(
        PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[1], .byte4 = 0x06, .color = {0xff, 0x00, 0x00}});
    sendControlTransfer(
        PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[2], .byte4 = 0x01, .color = {0x02, 0x00, 0x00}});
    sendControlTransfer(
        PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[3], .byte4 = 0x06, .color = {0x00, 0xff, 0x00}});
    sendControlTransfer(
        PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[4], .byte4 = 0x01, .color = {0xff, 0x00, 0x00}});
    sendControlTransfer(
        PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[5], .byte4 = 0x06, .color = {0xff, 0x00, 0x00}});
    sendControlTransfer(
        PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[6], .byte4 = 0x01, .color = {0x02, 0x00, 0x00}});
    sendControlTransfer(
        PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[7], .byte4 = 0x06, .color = {0xff, 0x00, 0x00}});
    sendControlTransfer(
        PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[8], .byte4 = 0x01, .color = {0x02, 0x00, 0x00}});
    sendControlTransfer(
        PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[9], .byte4 = 0x06, .color = {0xff, 0x00, 0x00}});
    sendControlTransfer(PacketPayload{
        .kind = LIGHT_DATA, .subType = lightDataSubtypes[10], .byte4 = 0x01, .color = {0x02, 0x00, 0x00}});
    sendControlTransfer(PacketPayload{.kind = RESET_CONTROL, .subType = REFRESH_LIGHT_STATE});
    sendControlTransfer(PacketPayload{.kind = BEGIN_END_PAYLOAD, .subType = endSubtype});
  }

  void trace(Intensity intensity) {
    setPattern(TRACE);
    setIntensity(intensity);
    refresh();
  }

  void breathing(RGBColor color, Intensity intensity, Speed speed) {
    setSecondaryPattern(BREATHING, color, speed);
    setIntensity(intensity);
    refresh();
  }

  void rainbow(Intensity intensity, Speed speed) {
    setSecondaryPattern(RAINBOW, BLACK, speed);
    setIntensity(intensity);
    refresh();
  }

  void solidColor(RGBColor color, Intensity intensity) {
    setSecondaryPattern(SOLID_COLOR, color, STOP);
    setIntensity(intensity);
    refresh();
  }

  void wave(Intensity intensity, Speed speed) {
    sendControlTransfer(PacketPayload{
        .kind = LIGHT_DATA, .subType = lightDataSubtypes[3], .byte4 = 0x06, .pattern = WAVE, .speed = speed});
    setIntensity(intensity);
    refresh();
  }

  void reactive(Intensity intensity) {
    setPattern(REACTIVE);
    setIntensity(intensity);
    refresh();
  }

  void flash(RGBColor color, Intensity intensity, Speed speed) {
    setSecondaryPattern(FLASH, color, speed);
    setIntensity(intensity);
    refresh();
  }
};

void sendLightPackets(const DeviceHandle& deviceHandle) {
  auto s = PacketSender(deviceHandle);
  s.breathing(RED, HIGH, FAST);
}

#endif  // USB_SEND_SENDPACKETS_H
