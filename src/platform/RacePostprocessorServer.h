#ifndef __RACE_POSTPROCESSOR_SERVER_H__
#define __RACE_POSTPROCESSOR_SERVER_H__

#include <Poco/Util/ServerApplication.h>

namespace RGT::Postprocessor
{

class RacePostprocessorServer : public Poco::Util::ServerApplication
{
public:
    void initialize(Application & self) final;

    void uninitialize() final;

    int main(const std::vector<std::string> &) final;
};

} // namespace RGT::Postprocessor

#endif // __RACE_POSTPROCESSOR_SERVER_H__