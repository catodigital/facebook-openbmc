#ifndef _BIC_RETIMER_H_
#define _BIC_RETIMER_H_
#include "fw-util.h"
#include "server.h"
#include "expansion.h"

using std::string;

enum class VendorID
{
  ASTERA_LABS = 0,
  MONTAGE = 1,
  UNKNOWN_VENDOR = 0xff,
};

class RetimerFwComponent : public Component {
  uint8_t slot_id;
  uint8_t fw_comp;
  string board;
  Server server;
  ExpansionBoard expansion;
  private:
    int get_ver_str(string& s);
    int update_internal(const std::string& image, bool force);
    int get_vendor_id(VendorID* vendor_id);
    bool update_validation(const std::string& image);
    bool is_recovery();
  public:
    RetimerFwComponent(const string& fru, const string& comp, const string& brd, uint8_t comp_id)
      : Component(fru, comp), slot_id(fru.at(4) - '0'), fw_comp(comp_id), board(brd),
        server(slot_id, fru), expansion(slot_id, fru, brd, fw_comp) {}
    int print_version();
    int get_version(json& j) override;
    int update(std::string image) override;
    int fupdate(std::string image) override;
};

#endif
