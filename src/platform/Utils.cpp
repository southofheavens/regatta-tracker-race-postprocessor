#include <Utils.h>

#include <RGT/Devkit/RGTException.h>

#include <iostream>
#include <iomanip>

#include <Poco/Redis/PoolableConnectionFactory.h>
#include <Poco/Redis/Type.h>
#include <Poco/XML/XMLWriter.h>
#include <Poco/SAX/AttributesImpl.h>

#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>

namespace
{

/// @brief Извлекает из Redis список с координатами участника
/// @param pc Установленное соединение с Redis
/// @param userId ID участника, список с координатами которого необходимо извлечь
/// @return Poco::Redis::Array массив с координатами
Poco::Redis::Array 
getParticipantCoordinates
(
    Poco::Redis::Client::Ptr clientPtr,
    const uint64_t & userId
)
{
    if (not clientPtr->isConnected()) 
    {
        // TODO лог
        throw std::exception{};
    }

    Poco::Redis::Array cmd;
    cmd << "LRANGE" << std::format("user_participation:{}", userId) 
        << "1" /* начинаем с 1 потому, что элемент с индексом 0 содержит строку "init" */
        << "-1";
    return clientPtr->execute<Poco::Redis::Array>(cmd);
}

using RedisClientObjectPool = Poco::ObjectPool<Poco::Redis::Client, Poco::Redis::Client::Ptr>;

/// @brief Извлекает из Redis списки с координатами участников
/// @param participantsIds Вектор с id участников
/// @param redisPool Пул соединений с Redis
/// @return Вектор с парами вида user_id - список координат
std::vector<std::pair<uint64_t, Poco::Redis::Array>>
getParticipantsCoordinates
(
    const std::vector<uint64_t> & participantsIds,
    RedisClientObjectPool & redisPool
)
{
    Poco::Redis::PooledConnection pc(redisPool, 500);
    Poco::Redis::Client::Ptr clientPtr = static_cast<Poco::Redis::Client::Ptr>(pc);
    if (clientPtr == nullptr or not clientPtr->isConnected()) 
    {
        // TODO лог
        // перезагрузить redis?
        throw std::exception{};
    }

    std::vector<std::pair<uint64_t, Poco::Redis::Array>> result;
    for (const uint64_t & id : participantsIds)
    {
        Poco::Redis::Array coordinates = getParticipantCoordinates(pc, id);
        result.push_back({id,coordinates});
    }
    return result;
}

/// @brief Удаляет из redis ключи вида user_participation:{id},
/// где id берутся из вектора participantsIds
/// @throw std::runtime_error при ошибке удаления
void
deleteParticipantsCoordinates
(
    const std::vector<uint64_t> & participantsIds,
    RedisClientObjectPool & redisPool
)
{
    Poco::Redis::PooledConnection pc(redisPool, 500);
    Poco::Redis::Client::Ptr clientPtr = static_cast<Poco::Redis::Client::Ptr>(pc);
    if (clientPtr == nullptr or not clientPtr->isConnected())
    {
        // TODO лог
        // перезагрузить redis?
        throw std::exception{};
    }

    Poco::Redis::Array cmd;
    cmd << "DEL";
    for (const uint64_t & id : participantsIds) {
        cmd << std::format("user_participation:{}", id);
    }

    Poco::Int64 result = clientPtr->execute<Poco::Int64>(cmd);

    if (result != participantsIds.size()) {
        throw std::runtime_error("error while deleting users participations from redis");
    }
}

struct Trackpoint
{
    double longitude;
    double latitude;
    uint64_t microsecondsSinceEpoch;
};

Trackpoint parseTrackpoint(const std::string & entry)
{
    // Проверка ошибок опускается по той причине, что логика обработки запроса
    // приёма данных не пропустит некорректную запись о координатах и отклонит запрос,
    // а, следовательно, все записи, полученные из Redis, являются корректными

    uint64_t firstSemicolonPos = entry.find(';');
    uint64_t secondSemicolonPos = entry.find(';', firstSemicolonPos + 1);

    std::cout << entry << '\n';

    double longitude = std::stod(entry.substr(0, firstSemicolonPos));
    double latitude = std::stod(entry.substr(firstSemicolonPos + 1, secondSemicolonPos));
    uint64_t microseconds = std::stoull(entry.substr(secondSemicolonPos + 1));

    return Trackpoint
    {
        .longitude = longitude, 
        .latitude = latitude,
        .microsecondsSinceEpoch = microseconds
    };
}

std::vector<Trackpoint> parseParticipantTrackpoints(const Poco::Redis::Array & entries)
{
    if (entries.isNull()) {
        return {};
    }

    std::vector<Trackpoint> trackpoints;
    for (const Poco::Redis::RedisType::Ptr & typePtr : entries)
    {   
        // Здесь опускаем проверки на typePtr->isBulkString() и на typeBulkString.value().isNull()  
        // по той же причине, что и в функции parseTrackpoint

        const Poco::Redis::Type<Poco::Redis::BulkString> & typeBulkString = 
            dynamic_cast<const Poco::Redis::Type<Poco::Redis::BulkString> &>(*typePtr);

        const std::string & entry = typeBulkString.value().value();
        trackpoints.push_back(parseTrackpoint(entry));
    }

    return trackpoints;
}

std::vector<std::pair<uint64_t, std::vector<Trackpoint>>>
parseParticipantsTrackpoints
(
    const std::vector<std::pair<uint64_t, Poco::Redis::Array>> & participantCoordinates
)
{
    std::vector<std::pair<uint64_t, std::vector<Trackpoint>>> participantsTrackpoints;
    participantsTrackpoints.reserve(participantCoordinates.size());

    for (const auto & [id, array] : participantCoordinates) {
        participantsTrackpoints.push_back({id, parseParticipantTrackpoints(array)});
    }

    return participantsTrackpoints;
}

std::string generateGpxFromCoordinates(const std::vector<Trackpoint> & trackpoints)
{
    std::ostringstream oss;

    using XMLOptions = Poco::XML::XMLWriter::Options;
    Poco::XML::XMLWriter writer(oss, XMLOptions::WRITE_XML_DECLARATION | XMLOptions::PRETTY_PRINT);

    writer.startDocument();

    Poco::XML::AttributesImpl gpxAttrs;
    gpxAttrs.addAttribute("", "version", "version", "CDATA", "1.1");
    gpxAttrs.addAttribute("", "xmlns", "xmlns", "CDATA", "http://www.topografix.com/GPX/1/1");
    writer.startElement("", "gpx", "gpx", gpxAttrs);

    writer.startElement("", "trk", "trk", Poco::XML::AttributesImpl());

    writer.startElement("", "name", "name", Poco::XML::AttributesImpl());
    writer.characters("My Race Track");
    writer.endElement("", "name", "name");

    writer.startElement("", "trkseg", "trkseg", Poco::XML::AttributesImpl());

    auto formatCoord = [](const double value, const uint8_t & precision = 6) -> std::string
    {
        std::ostringstream osstream;
        osstream << std::fixed << std::setprecision(precision) << value;
        return osstream.str();
    };

    for (const Trackpoint & trkpt : trackpoints)
    {
        Poco::XML::AttributesImpl trkptAttrs;
        trkptAttrs.addAttribute("", "lat", "lat", "CDATA", formatCoord(trkpt.latitude));
        trkptAttrs.addAttribute("", "lon", "lon", "CDATA", formatCoord(trkpt.longitude));

        writer.startElement("", "trkpt", "trkpt", trkptAttrs);

        writer.startElement("", "time", "time", Poco::XML::AttributesImpl());
        std::string timeStr = Poco::DateTimeFormatter::format(
            Poco::DateTime(Poco::Timestamp(trkpt.microsecondsSinceEpoch)),
            Poco::DateTimeFormat::ISO8601_FORMAT
        );
        writer.characters(timeStr);

        writer.endElement("", "time", "time");

        writer.endElement("", "trkpt", "trkpt");
    }

    writer.endElement("", "trkseg", "trkseg");
    writer.endElement("", "trk", "trk");
    writer.endElement("", "gpx", "gpx");

    writer.endDocument();

    return oss.str();
}

} // namespace 

namespace RGT::Postprocessor
{

bool postprocessorMessageHandler(const std::string & message, SubsystemsForConsume & subsystems)
{
    uint64_t raceId;
    try {
        raceId = std::stoull(message);
    }
    catch (const std::exception & e) {
        // ЛОГ
        throw;
    }

    Poco::Data::Session session = subsystems.psqlSubsystem.getPool().get();

    session <<
    "UPDATE races "
    "SET end_of_the_race = NOW() " /* TODO переименовать в end_time */
    "WHERE id = $1;",
    Poco::Data::Keywords::use(raceId),
    Poco::Data::Keywords::now;

    session <<
        "UPDATE races "
        "SET status = 'finished' "
        "WHERE id = $1;",
        Poco::Data::Keywords::use(raceId),
        Poco::Data::Keywords::now;
    
    std::vector<uint64_t> participantsIds;
    session << 
        "SELECT user_id "
        "FROM participations "
        "WHERE race_id = $1 AND role = 'participant';",
        Poco::Data::Keywords::use(raceId),
        Poco::Data::Keywords::into(participantsIds),
        Poco::Data::Keywords::now;

    session.close();

    // Извлекаем из Redis координаты каждого участника гонки
    std::vector<std::pair<uint64_t, Poco::Redis::Array>> usersCoordinates = 
        getParticipantsCoordinates(participantsIds, subsystems.redisSubsystem.getPool());
    std::vector<std::pair<uint64_t, std::vector<Trackpoint>>> usersTrackpoints = 
        parseParticipantsTrackpoints(usersCoordinates);

    Aws::S3::S3Client & s3Client = subsystems.s3Subsystem.getS3Client();
    // Генерируем GPX'ы и заливаем их в minio
    for (const auto & [userId, trackpoints] : usersTrackpoints)
    {
        Aws::S3::Model::PutObjectRequest putRequest;
        putRequest.SetKey(std::format("race_{}/user_{}.gpx", raceId, userId));
        putRequest.SetBucket("gpx-files");

        std::shared_ptr<Aws::StringStream> inputData = Aws::MakeShared<Aws::StringStream>("UploadHandlerInputStream");
        *inputData << generateGpxFromCoordinates(trackpoints);
        putRequest.SetBody(inputData);
        putRequest.SetContentType("application/gpx+xml");

        Aws::S3::Model::PutObjectOutcome outcome = s3Client.PutObject(putRequest);
    }

    AmqpClient::BasicMessage::ptr_t msg = AmqpClient::BasicMessage::Create(std::to_string(raceId));
    msg->DeliveryMode(AmqpClient::BasicMessage::dm_persistent);
    subsystems.rabbitmqSubsystem.getChannel().BasicPublish("", "analytics_tasks", msg);

    deleteParticipantsCoordinates(participantsIds, subsystems.redisSubsystem.getPool());

    return true;
}

} // namespace RGT::Postprocessor

// Если пользователь не загрузил координаты то при попытке составить для него gpx-файл всё падает 