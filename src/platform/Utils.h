#pragma once

#include <RGT/Devkit/Subsystems/RabbitMQSubsystem.h>
#include <RGT/Devkit/Subsystems/PsqlSubsystem.h>
#include <RGT/Devkit/Subsystems/RedisSubsystem.h>
#include <RGT/Devkit/Subsystems/S3Subsystem.h>
#include <Poco/Runnable.h>

namespace RGT::Postprocessor
{

struct SubsystemsForConsume
{
    Devkit::Subsystems::PsqlSubsystem & psqlSubsystem;
    Devkit::Subsystems::S3Subsystem & s3Subsystem;
    Devkit::Subsystems::RedisSubsystem & redisSubsystem;
    Devkit::Subsystems::RabbitMQSubsystem & rabbitmqSubsystem;
};

bool postprocessorMessageHandler(const std::string & message, SubsystemsForConsume & subsystems);

/// @brief Прослушивает очередь сообщений и вызывает для каждого успешно
/// принятого сообщения messageHandler. messageHandler возвращает true, если
/// сообщение было успешно обработано и false в противном случае
template<typename Context>
inline void consume
(
    AmqpClient::Channel & channel, 
    const std::string & queueName, 
    Context & messageHandlerContext, 
    std::function<bool(const std::string &, Context &)> messageHandler
)
{
    std::string consumerTag = channel.BasicConsume(queueName, "", false, false, false, 1);
    
    while (true)
    {
        AmqpClient::Envelope::ptr_t envelope;
        
        if (not channel.BasicConsumeMessage(consumerTag, envelope)) {
            throw std::runtime_error("BasicConsumeMessage failed");
        }

        std::string receivedMessage = envelope->Message()->Body();
        
        std::cout << "ПОЛУЧЕНО СООБЩЕНИЕ: " << receivedMessage << std::endl;
        
        try 
        {
            if (messageHandler(receivedMessage, messageHandlerContext)) {
                channel.BasicAck(envelope); 
            } 
            else {
                channel.BasicReject(envelope, false); 
            }
        } 
        catch (const std::exception & e) {
            channel.BasicReject(envelope, false);
        }
    }
}
    
} // namespace RGT::Postprocessor
