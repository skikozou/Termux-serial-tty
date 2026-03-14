/** @brief USBUART driver for USB CDC ACM devices (e.g. iRobot Roomba 980)
 *  @file  cdc_acm.cpp
 *
 *  Roomba 980 (VID 27a6:0002) のエンドポイント構成:
 *    ifc=0 class=0x02 (CDC Control)  ep=0x82 (Interrupt IN)
 *    ifc=1 class=0x0a (CDC Data)     ep=0x81 (Bulk IN), ep=0x03 (Bulk OUT)
 */
#include "usbuart.hpp"
#include <endian.h>
#include <libusb.h>
namespace usbuart {
// CDC ACM クラス定義 (USB CDC spec 1.1)
static constexpr uint8_t CDC_SET_LINE_CODING = 0x20;
static constexpr uint8_t CDC_SET_CONTROL_LINE_STATE = 0x22;
static constexpr uint8_t CDC_REQTYPE_OUT = LIBUSB_REQUEST_TYPE_CLASS |
                                           LIBUSB_RECIPIENT_INTERFACE |
                                           LIBUSB_ENDPOINT_OUT;
// SET_LINE_CODING のペイロード (7バイト, リトルエンディアン)
struct __attribute__((packed)) cdc_line_coding {
  uint32_t dwDTERate;  // ボーレート
  uint8_t bCharFormat; // ストップビット: 0=1, 1=1.5, 2=2
  uint8_t bParityType; // パリティ: 0=None, 1=Odd, 2=Even
  uint8_t bDataBits;   // データビット: 5,6,7,8
};
class cdc_acm : public generic {
public:
  static const struct interface _ifc;
  void setbaudrate(baudrate_t baudrate) const throw(error_t) {
    (void)baudrate;
  }
  void setup(const eia_tia_232_info &info) const throw(error_t) {
    set_line_coding(info);
    set_control_line_state(true);
    generic::setup(info);
  }
  void reset() const throw(error_t) {}
  void sendbreak() const throw(error_t) {}
  void read_callback(libusb_transfer *, size_t &pos) noexcept { pos = 0; }
  ~cdc_acm() noexcept {}
private:
  inline cdc_acm(libusb_device_handle *d, uint8_t ifnum) throw(error_t)
      : generic(d, _ifc, ifnum) {}
  void set_line_coding(const eia_tia_232_info &info) const throw(error_t) {
    cdc_line_coding lc;
    lc.dwDTERate = htole32(info.baudrate);
    lc.bCharFormat = (info.stopbits == stop_bits_t::two) ? 2 : 0;
    lc.bParityType = (info.parity == parity_t::odd)    ? 1
                     : (info.parity == parity_t::even) ? 2
                                                       : 0;
    lc.bDataBits = info.databits;
    int r = libusb_control_transfer(
        dev, CDC_REQTYPE_OUT, CDC_SET_LINE_CODING, 0, 0,
        reinterpret_cast<unsigned char *>(&lc), sizeof(lc), timeout);
    if (r < 0) {
      log.e(__, "SET_LINE_CODING failed: %s", libusb_error_name(r));
      throw error_t::control_error;
    }
  }
  void set_control_line_state(bool dtr) const throw(error_t) {
    int r = libusb_control_transfer(dev, CDC_REQTYPE_OUT,
                                    CDC_SET_CONTROL_LINE_STATE,
                                    dtr ? 0x01 : 0x00,
                                    0, nullptr, 0, timeout);
    if (r < 0) {
      log.e(__, "SET_CONTROL_LINE_STATE failed: %s", libusb_error_name(r));
      throw error_t::control_error;
    }
  }
  static class factory : driver::factory {
    driver *create(libusb_device_handle *, uint8_t) const throw(error_t);
  } _factory;
};
const struct interface cdc_acm::_ifc = {0x81, 0x03, 64};
cdc_acm::factory cdc_acm::_factory;
driver *cdc_acm::factory::create(libusb_device_handle *handle,
                                 uint8_t ifc) const throw(error_t) {
  static constexpr const uint32_t table[] = {
      devid32(0x27a6, 0x0002), // iRobot Roomba 980
  };
  device_id did = devid(handle);
  uint32_t id = devid32(did);
  if (!id) return nullptr;
  bool found = false;
  for (auto &&i : table) {
    if ((found = (i == id))) break;
  }
  if (!found) return nullptr;
  log.i(__, "probing %s for %04x:%04x", "cdc_acm", did.vid, did.pid);

  // ifc=0 (CDC Control) のカーネルドライバをデタッチ＆クレーム
  libusb_detach_kernel_driver(handle, 0); // エラー無視
  int r0 = libusb_claim_interface(handle, 0);
  if (r0 < 0) {
    log.e(__, "claim ifc=0 failed: %s", libusb_error_name(r0));
    throw error_t::interface_busy;
  }

  // ifc=1 (CDC Data) のカーネルドライバをデタッチ＆クレーム
  int rd1 = libusb_detach_kernel_driver(handle, 1);
  log.i(__, "detach ifc=1 result: %d %s", rd1, libusb_error_name(rd1));
  int r1 = libusb_claim_interface(handle, 1);
  if (r1 < 0) {
    log.e(__, "claim ifc=1 failed: %s", libusb_error_name(r1));
    libusb_release_interface(handle, 0);
    throw error_t::interface_busy;
  }

  // claim済みなので claim_interface() は呼ばない
  return new cdc_acm(handle, 1);
}
} // namespace usbuart
