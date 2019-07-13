#ifndef USB_SEND_LIBUSBWRAPPER_H
#define USB_SEND_LIBUSBWRAPPER_H

#include <exception>
#include <functional>
#include <iostream>
#include <string>

#include <libusb-1.0/libusb.h>

class USBContext {
  libusb_context* ctx{};

 public:
  USBContext() {
    int err = libusb_init(&ctx);
    if (err != 0) {
      throw std::runtime_error(std::string("libusb_init failed: ") + libusb_error_name(err));
    }
  }

  ~USBContext() {
    // FIXME: not good
//    libusb_exit(ctx);
  }

  void setLogLevel(libusb_log_level level) const noexcept { libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, level); }

  libusb_context* getContext() const noexcept { return ctx; }
};

class DeviceHandle {
  libusb_device_handle* handle{};

 public:
  explicit DeviceHandle(libusb_device* dev) {
    int err = libusb_open(dev, &handle);
    if (err != 0) {
      throw std::runtime_error(std::string("libusb_open failed: ") + libusb_error_name(err));
    }
  }

  ~DeviceHandle() { libusb_close(handle); }

  libusb_device_handle* getHandle() const noexcept { return handle; }
};

libusb_device_descriptor getDeviceDescriptor(libusb_device* dev) {
  libusb_device_descriptor desc{};
  int err = libusb_get_device_descriptor(dev, &desc);
  if (err < 0) {
    throw std::runtime_error(std::string("libusb_get_device_descriptor failed: ") + libusb_error_name(err));
  }
  return desc;
}

class DeviceList {
  libusb_device** devs{};
  ssize_t size;

 public:
#pragma clang diagnostic push
#pragma ide diagnostic ignored "performance-unnecessary-value-param"
  explicit DeviceList(USBContext ctx) {
    size = libusb_get_device_list(ctx.getContext(), &devs);
    if (size < 0) {
      throw std::runtime_error(std::string("libusb_get_device_list failed: ") + libusb_error_name(size));
    }
  }
#pragma clang diagnostic pop

  ~DeviceList() { libusb_free_device_list(devs, true); }

  libusb_device** begin() { return devs; }

  libusb_device** end() { return nullptr; }
};

#endif  // USB_SEND_LIBUSBWRAPPER_H
