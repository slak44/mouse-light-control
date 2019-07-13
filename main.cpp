#include <array>
#include <cstring>
#include <iostream>

#include <errno.h>
#include <unistd.h>

#include <libusb-1.0/libusb.h>

#include "libusbWrapper.h"
#include "sendPackets.h"

using namespace std;

bool detachKernel(libusb_device_handle* handle, int interface) {
  int res = libusb_kernel_driver_active(handle, interface);
  if (res < 0) {
    cerr << "libusb_kernel_driver_active failed: " << libusb_error_name(res) << endl;
    return false;
  }
  if (res == 1) {
    cout << "Kernel driver on (interface " << interface << ")" << endl;
    res = libusb_detach_kernel_driver(handle, interface);
    if (res != 0) {
      cerr << "libusb_detach_kernel_driver failed: " << libusb_error_name(res) << endl;
      return false;
    } else {
      cout << "Kernel driver detached (interface " << interface << ")" << endl;
    }
  } else {
    cout << "Kernel driver off (interface " << interface << ")" << endl;
  }
  return true;
}

bool claimInterface(libusb_device_handle* handle, int interface) {
  int err = libusb_claim_interface(handle, interface);
  if (err != 0) {
    cerr << "libusb_claim_interface failed: " << libusb_error_name(err) << endl;
    return false;
  }
  return true;
}

void releaseInterface(libusb_device_handle* handle, int interface) {
  int err = libusb_release_interface(handle, interface);
  if (err != 0) {
    cerr << "libusb_release_interface failed: " << libusb_error_name(err) << endl;
  }
}

void attachKernel(libusb_device_handle* handle, int interface) {
  int err = libusb_attach_kernel_driver(handle, interface);
  if (err != 0) {
    cerr << "libusb_attach_kernel_driver failed: " << libusb_error_name(err) << endl;
  }
}

void usbSetup(libusb_device* dev) {
  auto dh = DeviceHandle(dev);
  bool iface0ok = detachKernel(dh.getHandle(), 0);
  bool iface2ok = detachKernel(dh.getHandle(), 2);
  if (!iface0ok || !iface2ok) return;
  iface0ok = claimInterface(dh.getHandle(), 0);
  iface2ok = claimInterface(dh.getHandle(), 2);
  if (iface0ok && iface2ok) {
    sendLightPackets(dh.getHandle());
  }
  releaseInterface(dh.getHandle(), 0);
  releaseInterface(dh.getHandle(), 2);
  attachKernel(dh.getHandle(), 0);
  attachKernel(dh.getHandle(), 2);

  //  err = libusb_reset_device(handle);
  //  if (err != 0) {
  //    cerr << "libusb_reset_device failed: " << libusb_error_name(err) << endl;
  //  }
}

int main() {
  auto ctx = USBContext();
  ctx.setLogLevel(LIBUSB_LOG_LEVEL_INFO);

  for (auto dev : DeviceList(ctx)) {
    auto desc = getDeviceDescriptor(dev);
    if (desc.idVendor == 0x04d9 && desc.idProduct == 0xfc3a) {
      usbSetup(dev);
      break;
    }
  }

  return 0;
}
