#ifndef STACK_PHY_MEDIUM_CELLULARBACKGROUNDNOISE_H_
#define STACK_PHY_MEDIUM_CELLULARBACKGROUNDNOISE_H_

#include <inet/common/Module.h>
#include <inet/physicallayer/wireless/common/contract/packetlevel/IBackgroundNoise.h>

namespace simu5g {

// SPIKE: a background noise that adds none
class CellularBackgroundNoise : public inet::Module, public inet::physicallayer::IBackgroundNoise
{
  public:
    virtual const inet::physicallayer::INoise *computeNoise(const inet::physicallayer::IListening *listening) const override { return nullptr; }
    virtual std::ostream& printToStream(std::ostream& stream, int level, int evFlags = 0) const override { return stream << "CellularBackgroundNoise"; }
};

} // namespace simu5g

#endif
