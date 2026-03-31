#include <RacePostprocessorServer.h>
#include <Utils.h>

#include <Poco/Data/PostgreSQL/Connector.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPResponse.h>

#include <rgt/devkit/RGTException.h>
#include <rgt/devkit/General.h>
#include <rgt/devkit/subsystems/S3Subsystem.h>
#include <rgt/devkit/subsystems/PsqlSubsystem.h>
#include <rgt/devkit/subsystems/RedisSubsystem.h>
#include <rgt/devkit/subsystems/RabbitMQSubsystem.h>

#include <aws/core/Aws.h>

#include <Poco/Util/JSONConfiguration.h>

namespace RGT::Postprocessor
{

void RacePostprocessorServer::initialize(Poco::Util::Application & self)
{
    loadConfiguration();

    RGT::Devkit::readDotEnv();

    Poco::Util::Application::addSubsystem(new RGT::Devkit::Subsystems::PsqlSubsystem());
    Poco::Util::Application::addSubsystem(new RGT::Devkit::Subsystems::S3Subsystem());
    Poco::Util::Application::addSubsystem(new RGT::Devkit::Subsystems::RedisSubsystem());
    Poco::Util::Application::addSubsystem(new RGT::Devkit::Subsystems::RabbitMQSubsystem());

    ServerApplication::initialize(self);
}

void RacePostprocessorServer::uninitialize()
{ ServerApplication::uninitialize(); }

int RacePostprocessorServer::main(const std::vector<std::string>&)
{
    Poco::Util::LayeredConfiguration & cfg = RacePostprocessorServer::config();

    SubsystemsForConsume subsystems = 
    {
        .psqlSubsystem = Poco::Util::Application::getSubsystem<Devkit::Subsystems::PsqlSubsystem>(),
        .s3Subsystem = Poco::Util::Application::getSubsystem<Devkit::Subsystems::S3Subsystem>(),
        .redisSubsystem = Poco::Util::Application::getSubsystem<Devkit::Subsystems::RedisSubsystem>(),
        .rabbitmqSubsystem = Poco::Util::Application::getSubsystem<Devkit::Subsystems::RabbitMQSubsystem>()
    };

    consume<SubsystemsForConsume>(subsystems.rabbitmqSubsystem.getAmqpConnection("postprocessor"), 
        "postprocessor_tasks", subsystems, postprocessorMessageHandler);

    waitForTerminationRequest(); // никогда не выполнится

    return Application::EXIT_OK;
}

} // namespace RGT::Postprocessor
