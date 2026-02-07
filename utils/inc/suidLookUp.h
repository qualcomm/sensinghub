/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#pragma once

#include <vector>
#include <chrono>
#include <memory>
#include <mutex>
#include <condition_variable>
#include "ISession.h"
#include "SessionFactory.h"

using suid = com::quic::sensinghub::suid;

using com::quic::sensinghub::session::V1_0::ISession;
using com::quic::sensinghub::session::V1_0::sessionFactory;
/**
 * @brief type alias for an suid event function
 *
 * param datatype: datatype of of the sensor associated with the
 * event
 * param suids: vector of suids available for the given datatype
 */
using suidEventCb =
    std::function<void(const std::string& datatype,
                       const std::vector<suid>& suids)>;

/**
 * @brief Utility class for discovering available sensors using
 *        dataytpe
 *
 */
class suidLookUp
{
public:
    /**
     * @brief creates a new connection to qsh for suid lookup
     *
     * @param cb callback function for suids
     */
    suidLookUp(suidEventCb cb, int hubId = -1);
    ~suidLookUp();

    /**
     *  @brief look up the suid for a given datatype, registered
     *         callback will be called when suid is available for
     *         this datatype
     *
     *  @param datatype data type for which suid is requested
     *  @param default_only option to ask for publishing only default
     *         suid for the given data type. default value is false
     */
    void requestSuid(std::string datatype, bool defaultOnly = false);

private:
    suidEventCb mEventCb;
    void handleQshEvent(const uint8_t *data, size_t size, uint64_t timeStamp);
    std::unique_ptr<ISession> mSession = nullptr;
    std::unique_ptr<sessionFactory> mSessionFactory = nullptr;
    suid mSensorUid;
    bool mSetThreadName = false;
};

/**
 * @brief Utility class for synchronous sensor UID lookup operations
 *
 * The locate class provides a simplified interface for looking up sensor UIDs
 * for a given datatype. It performs synchronous lookups with timeout support
 * and handles the underlying asynchronous SUID discovery mechanism internally.
 *
 * @note This class is designed for single-use lookup operations. Each instance
 *       should be used for one lookup operation only.
 */
class locate
{
public:
    /**
     * @brief Default constructor for the locate class
     *
     * Initializes a new instance of the locate class for performing SUID lookup
     * operations. The instance is prepared for a single lookup operation.
     */
    locate();

    /**
     * @brief Performs synchronous lookup of sensor UIDs for a given datatype
     *
     * This method initiates a SUID lookup request for the specified datatype and
     * blocks until either the lookup completes or the timeout expires. It returns
     * a reference to a vector containing all discovered sensor UIDs for the datatype.
     *
     * @param datatype The sensor datatype to lookup (e.g., "accel", "gyro", "mag")
     * @param hub_id The sensor hub ID to query. Use -1 for default hub (default: -1)
     * @param timeout_ms Maximum time to wait for lookup completion (default: 5000ms)
     *
     * @return Shared pointer to vector of sensor UIDs found for the specified datatype.
     *         Returns empty vector if no sensors found or timeout occurred.
     *
     * @warning This method blocks the calling thread until completion or timeout
     *
     * @see suidLookUp::requestSuid() for the underlying asynchronous implementation
     */
    std::shared_ptr<std::vector<suid>> lookUp(const std::string& dataYype, int hubId = -1,
        std::chrono::milliseconds timeoutMs = DEFAULT_SUID_LOOKUP_TIMEOUT);

private:
    void onSuidsAvailable(const std::string& datatype,
        const std::vector<suid>& suids);

    static constexpr auto DEFAULT_SUID_LOOKUP_TIMEOUT = std::chrono::milliseconds(1000);

    std::shared_ptr<std::vector<suid>> mSuids;

    bool mLookupDone = false;
    std::mutex mMutex;
    std::condition_variable mConditionVar;
};
