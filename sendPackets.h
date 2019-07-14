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

// LIGHT_DATA subtype values when subType2 is 0x04
constexpr Byte lightDataSubtypes[] = {0x46, 0x49, 0x4f, 0x51, 0x57, 0x59, 0x5f, 0x61, 0x67, 0x69, 0x6f};
// LIGHT_DATA subtype values when subType2 is 0x00
constexpr Byte lightDataSubtypes2[] = {0x32, 0x38, 0x42 /* ??? */};

constexpr Byte beginSubtype = 0x00;
constexpr Byte endSubtype = 0x01;

enum ResetSubtypes : Byte {
  // Looks like it resets the mouse like libusb_reset_device
  RESET_DEVICE = 0x00,
  // Gives broken pipe
  BROKEN_PIPE = 0x01,
  // Appears to tell the mouse to refresh the current light animation state
  // Works with all the numbers from 0x02 to 0xff
  // Might be some sort of ACK, while using subType2 to specify what to acknowledge
  REFRESH_LIGHT_STATE = 0x02
};

struct RGBColor {
  Byte red = 0x00;
  Byte green = 0x00;
  Byte blue = 0x00;
};

constexpr RGBColor RED = {0xff, 0x00, 0x00};
constexpr RGBColor GREEN = {0x00, 0xff, 0x00};
constexpr RGBColor BLUE = {0x00, 0x00, 0xff};
constexpr RGBColor WHITE = {0xff, 0xff, 0xff};

enum Intensity : Byte { OFF = 0x00, LOW = 0x01, MEDIUM = 0x02, HIGH = 0x03 };

enum Speed : Byte { SLOW = 0x08, NORMAL = 0x05, FAST = 0x02, FAST_FLASH_ALT = 0x01 };

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
  Byte subType2 = 0x04;

  // The rest of the bytes seem to not be 0 only for LIGHT_DATA packets

  // Unknown byte
  // 0x02, 0x01, 0x06, 0x00, 0x04
  // Might be a tag specifying what the following bytes represent
  Byte byte4 = 0x00;
  // Unknown bytes
  // Might be RGB color, were never seen to differ from 0
  Byte bytes567[3] = {0x00, 0x00, 0x00};
  // Specifies either light color, intensity, or neither (zero'd)
  union {
    RGBColor color = {0x00, 0x00, 0x00};
    struct {
      Intensity intensity;
      Byte : 8;
      Byte : 8;
    };
  };
  // Magic bytes for what kind of light to set on
  Byte lightKind[3] = {0x00, 0x00, 0x00};
  // Unknown bytes
  // Never seen differ from zero; likely to just be struct padding
  Byte byte14 = 0x00;
  Byte byte15 = 0x00;
};

template <size_t DATA_SIZE = 16>
void sendControlTransfer(libusb_device_handle* handle, Byte data[]) {
  int res = libusb_control_transfer(
      handle,
      0x21,    // bmRequestType (host to device)
      0x09,    // bRequest (set configuration)
      0x0302,  // wValue (configuration value)
      0x0002,  // wIndex (specifies the target interface; wireshark captures the packet on interface 0, but this control
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
void sendControlTransfer(libusb_device_handle* handle, std::array<unsigned char, DATA_SIZE> data) {
  sendControlTransfer<DATA_SIZE>(handle, data.data());
}

void sendControlTransfer(libusb_device_handle* handle, PacketPayload payload) {
  sendControlTransfer(handle, reinterpret_cast<Byte*>(&payload));
}

void turnOff(libusb_device_handle* handle) {
  sendControlTransfer(handle, PacketPayload{.kind = BEGIN_END_PAYLOAD, .subType = beginSubtype});
  sendControlTransfer(handle, PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[0], .byte4 = 0x02});
  sendControlTransfer(handle, PacketPayload{.kind = LIGHT_DATA,
                                            .subType = lightDataSubtypes[1],
                                            .byte4 = 0x06,
                                            .color = {0xff, 0x00, 0x00},
                                            .lightKind = {0x01, 0x08, 0x04}});
  sendControlTransfer(
      handle,
      PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[2], .byte4 = 0x01, .color = {0x02, 0x00, 0x00}});
  sendControlTransfer(
      handle,
      PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[3], .byte4 = 0x06, .color = {0x00, 0xff, 0x00}});
  sendControlTransfer(
      handle,
      PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[4], .byte4 = 0x01, .color = {0xff, 0x00, 0x00}});
  sendControlTransfer(handle, PacketPayload{.kind = LIGHT_DATA,
                                            .subType = lightDataSubtypes[5],
                                            .byte4 = 0x06,
                                            .color = {0xff, 0x00, 0x00},
                                            .lightKind = {0x01, 0x08, 0x04}});
  sendControlTransfer(
      handle,
      PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[6], .byte4 = 0x01, .color = {0x02, 0x00, 0x00}});
  sendControlTransfer(handle, PacketPayload{.kind = LIGHT_DATA,
                                            .subType = lightDataSubtypes[7],
                                            .byte4 = 0x06,
                                            .color = {0xff, 0x00, 0x00},
                                            .lightKind = {0x01, 0x08, 0x04}});
  sendControlTransfer(
      handle,
      PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[8], .byte4 = 0x01, .color = {0x02, 0x00, 0x00}});
  sendControlTransfer(handle, PacketPayload{.kind = LIGHT_DATA,
                                            .subType = lightDataSubtypes[9],
                                            .byte4 = 0x06,
                                            .color = {0xff, 0x00, 0x00},
                                            .lightKind = {0x01, 0x08, 0x04}});
  sendControlTransfer(
      handle,
      PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[10], .byte4 = 0x01, .color = {0x02, 0x00, 0x00}});
  sendControlTransfer(handle, PacketPayload{.kind = RESET_CONTROL, .subType = REFRESH_LIGHT_STATE});
  sendControlTransfer(handle, PacketPayload{.kind = BEGIN_END_PAYLOAD, .subType = endSubtype});
}

void trace(libusb_device_handle* handle, Intensity intensity) {
  sendControlTransfer(handle, PacketPayload{.kind = BEGIN_END_PAYLOAD, .subType = beginSubtype});
  sendControlTransfer(handle, PacketPayload{.kind = LIGHT_DATA,
                                            .subType = lightDataSubtypes[3],
                                            .byte4 = 0x06,
                                            // First byte is trace pattern magic
                                            .lightKind = {0x06, 0x00, 0x00}});
  sendControlTransfer(
      handle,
      PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[4], .byte4 = 0x01, .intensity = intensity});
  sendControlTransfer(handle, PacketPayload{.kind = RESET_CONTROL, .subType = REFRESH_LIGHT_STATE});
  sendControlTransfer(handle, PacketPayload{.kind = BEGIN_END_PAYLOAD, .subType = endSubtype});
}

void breathing(libusb_device_handle* handle, RGBColor color, Intensity intensity, Speed speed) {
  sendControlTransfer(handle, PacketPayload{.kind = BEGIN_END_PAYLOAD, .subType = beginSubtype});
  sendControlTransfer(handle, PacketPayload{.kind = LIGHT_DATA,
                                            .subType = lightDataSubtypes[3],
                                            .byte4 = 0x06,
                                            .color = color,
                                            // These magic bytes make it a "breathing" pattern
                                            .lightKind = {0x01, speed, 0x04}});
  sendControlTransfer(
      handle,
      PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[4], .byte4 = 0x01, .intensity = intensity});
  sendControlTransfer(handle, PacketPayload{.kind = RESET_CONTROL, .subType = REFRESH_LIGHT_STATE});
  sendControlTransfer(handle, PacketPayload{.kind = BEGIN_END_PAYLOAD, .subType = endSubtype});
}

void rainbow(libusb_device_handle* handle, Intensity intensity, Speed speed) {
  sendControlTransfer(handle, PacketPayload{.kind = BEGIN_END_PAYLOAD, .subType = beginSubtype});
  sendControlTransfer(handle, PacketPayload{.kind = LIGHT_DATA,
                                            .subType = lightDataSubtypes[3],
                                            .byte4 = 0x06,
                                            // These magic bytes make it a "rainbow" pattern
                                            .lightKind = {0x01, speed, 0x08}});
  sendControlTransfer(
      handle,
      PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[4], .byte4 = 0x01, .intensity = intensity});
  sendControlTransfer(handle, PacketPayload{.kind = RESET_CONTROL, .subType = REFRESH_LIGHT_STATE});
  sendControlTransfer(handle, PacketPayload{.kind = BEGIN_END_PAYLOAD, .subType = endSubtype});
}

void solidColor(libusb_device_handle* handle, RGBColor color, Intensity intensity) {
  sendControlTransfer(handle, PacketPayload{.kind = BEGIN_END_PAYLOAD, .subType = beginSubtype});
  sendControlTransfer(handle, PacketPayload{.kind = LIGHT_DATA,
                                            .subType = lightDataSubtypes[3],
                                            .byte4 = 0x06,
                                            .color = color,
                                            // These magic bytes make it a solid color
                                            .lightKind = {0x01, 0x00, 0x02}});
  sendControlTransfer(
      handle,
      PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[4], .byte4 = 0x01, .intensity = intensity});
  sendControlTransfer(handle, PacketPayload{.kind = RESET_CONTROL, .subType = REFRESH_LIGHT_STATE});
  sendControlTransfer(handle, PacketPayload{.kind = BEGIN_END_PAYLOAD, .subType = endSubtype});
}

void wave(libusb_device_handle* handle, Intensity intensity, Speed speed) {
  sendControlTransfer(handle, PacketPayload{.kind = BEGIN_END_PAYLOAD, .subType = beginSubtype});
  sendControlTransfer(handle, PacketPayload{.kind = LIGHT_DATA,
                                            .subType = lightDataSubtypes[3],
                                            .byte4 = 0x06,
                                            // These magic bytes make it a wave pattern
                                            .lightKind = {0x02, speed, 0x00}});
  sendControlTransfer(
      handle,
      PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[4], .byte4 = 0x01, .intensity = intensity});
  sendControlTransfer(handle, PacketPayload{.kind = RESET_CONTROL, .subType = REFRESH_LIGHT_STATE});
  sendControlTransfer(handle, PacketPayload{.kind = BEGIN_END_PAYLOAD, .subType = endSubtype});
}

void reactive(libusb_device_handle* handle, Intensity intensity) {
  sendControlTransfer(handle, PacketPayload{.kind = BEGIN_END_PAYLOAD, .subType = beginSubtype});
  sendControlTransfer(handle, PacketPayload{.kind = LIGHT_DATA,
                                            .subType = lightDataSubtypes[3],
                                            .byte4 = 0x06,
                                            // These magic bytes make it a reactive pattern
                                            .lightKind = {0x07, 0x00, 0x00}});
  sendControlTransfer(
      handle,
      PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[4], .byte4 = 0x01, .intensity = intensity});
  sendControlTransfer(handle, PacketPayload{.kind = RESET_CONTROL, .subType = REFRESH_LIGHT_STATE});
  sendControlTransfer(handle, PacketPayload{.kind = BEGIN_END_PAYLOAD, .subType = endSubtype});
}

void flash(libusb_device_handle* handle, RGBColor color, Intensity intensity, Speed speed) {
  sendControlTransfer(handle, PacketPayload{.kind = BEGIN_END_PAYLOAD, .subType = beginSubtype});
  sendControlTransfer(handle, PacketPayload{.kind = LIGHT_DATA,
                                            .subType = lightDataSubtypes[3],
                                            .byte4 = 0x06,
                                            .color = color,
                                            // These magic bytes make it a flashing pattern
                                            .lightKind = {0x01, speed, 0x10}});
  sendControlTransfer(
      handle,
      PacketPayload{.kind = LIGHT_DATA, .subType = lightDataSubtypes[4], .byte4 = 0x01, .intensity = intensity});
  sendControlTransfer(handle, PacketPayload{.kind = RESET_CONTROL, .subType = REFRESH_LIGHT_STATE});
  sendControlTransfer(handle, PacketPayload{.kind = BEGIN_END_PAYLOAD, .subType = endSubtype});
}

void sendLightPackets(libusb_device_handle* handle) {
  flash(handle, WHITE, HIGH, FAST);
  sleep(3);
  turnOff(handle);
}

#endif  // USB_SEND_SENDPACKETS_H
