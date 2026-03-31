#ifndef __UTILS_H__
#define __UTILS_H__

#include <rgt/devkit/subsystems/RabbitMQSubsystem.h>
#include <rgt/devkit/subsystems/PsqlSubsystem.h>
#include <rgt/devkit/subsystems/RedisSubsystem.h>
#include <rgt/devkit/subsystems/S3Subsystem.h>
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
    const RGT::Devkit::Subsystems::RabbitMQSubsystem::AmqpConnection & connection, 
    const std::string & queueName, 
    Context & messageHandlerContext, 
    std::function<bool(const std::string &, Context &)> messageHandler
)
{
    amqp_basic_consume(connection.connection, connection.channel, amqp_cstring_bytes(queueName.c_str()), 
        amqp_empty_bytes, 0, 0, 0, amqp_empty_table);
    amqp_rpc_reply_t consumeResult = amqp_get_rpc_reply(connection.connection);
    if (consumeResult.reply_type != AMQP_RESPONSE_NORMAL) {
        throw std::runtime_error("consume failed");
    }

    while (true)
    {
        amqp_envelope_t env;
        amqp_maybe_release_buffers(connection.connection);
        amqp_rpc_reply_t consumeMsgResult = amqp_consume_message(connection.connection, &env, nullptr, 0);
        if (consumeMsgResult.reply_type != AMQP_RESPONSE_NORMAL) {
            throw std::runtime_error("consume message failed");
        }

        std::string receivedMessage((char*)env.message.body.bytes, env.message.body.len);
        uint64_t tag = env.delivery_tag;
        std::cout << "ПОЛУЧЕНО СООБЩЕНИЕ: " << receivedMessage << std::endl;
        if (messageHandler(receivedMessage, messageHandlerContext)) {
            amqp_basic_ack(connection.connection, connection.channel, tag, 0);
        }
        else {
            amqp_basic_nack(connection.connection, connection.channel, tag, 0, 0);
        }

        amqp_destroy_envelope(&env);
    }
}
    
} // namespace RGT::Postprocessor

#endif // __UTILS_H__
