const pool = require("../db");
const { DateTime } = require("luxon");

// Cache to store allowed fields (5 minutes TTL)
const sensorCache = new Map();
const CACHE_TTL = 300000; // 5 minutes in milliseconds

// Sensor to field mapping by data type
const SENSOR_FIELD_MAP = {
    raw: {
        temperature: "temperature",
        pressure: "pressure",
        humidity: "humidity",
        lux: "lux",
        uvi: "uvi",
        rain_gauge: "rain",
        anemometer: ["wind_speed", "gust_speed"],
        wind_vane: ["wind_direction", "gust_direction"],
    },
    hourly: {
        temperature: "avg_temperature",
        pressure: "avg_pressure",
        humidity: "avg_humidity",
        lux: "avg_lux",
        uvi: "avg_uvi",
        rain_gauge: "sum_rain",
        anemometer: [
            "avg_wind_speed",
            "max_gust_speed",
            "standard_deviation_speed",
        ],
        wind_vane: ["avg_wind_direction", "max_gust_direction"],
    },
    daily: {
        temperature: ["max_temperature", "min_temperature"],
        pressure: ["max_pressure", "min_pressure"],
        humidity: ["max_humidity", "min_humidity"],
        lux: ["max_lux", "min_lux"],
        uvi: "max_uvi",
        rain_gauge: "sum_rain",
        anemometer: [
            "avg_wind_speed",
            "max_gust_speed",
            "standard_deviation_speed",
        ],
        wind_vane: ["avg_wind_direction", "max_gust_direction"],
    },
};

// Base fields that are always present
const BASE_FIELDS = ["id", "station_id", "date"];

/**
 * Get active sensors for a station
 * @param {string} stationId - Station ID
 * @returns {Promise<Object>} Object with active sensors
 */
async function getStationSensors(stationId) {
    const { rows } = await pool.query(
        `SELECT temperature, pressure, humidity, lux, uvi,
            rain_gauge, anemometer, wind_vane
     FROM weather_stations
     WHERE station_id = $1`,
        [stationId],
    );
    return rows[0] || null;
}

/**
 * Get allowed fields for a station
 * @param {string} stationId - Station ID
 * @param {'raw'|'hourly'|'daily'} dataType - Data type
 * @returns {Promise<string[]>} Array of allowed fields
 */
async function getAllowedFields(stationId, dataType) {
    if (!["raw", "hourly", "daily"].includes(dataType)) {
        throw new Error("Invalid data type. Must be: raw, hourly or daily");
    }

    const cacheKey = `${stationId}-${dataType}`;

    // Check cache first
    if (sensorCache.has(cacheKey)) {
        return [...sensorCache.get(cacheKey)];
    }

    // Get station sensors
    const stationSensors = await getStationSensors(stationId);
    if (!stationSensors) {
        throw new Error(`Station ${stationId} not found`);
    }

    // Get specific fields for active sensors
    const sensorFields = Object.entries(stationSensors)
        .filter(
            ([sensor, enabled]) =>
                enabled && SENSOR_FIELD_MAP[dataType][sensor],
        )
        .flatMap(([sensor]) => {
            const fields = SENSOR_FIELD_MAP[dataType][sensor];
            return Array.isArray(fields) ? fields : [fields];
        });

    const uniqueFields = [...new Set([...BASE_FIELDS, ...sensorFields])];

    // Store in cache with TTL
    sensorCache.set(cacheKey, uniqueFields);
    setTimeout(() => sensorCache.delete(cacheKey), CACHE_TTL);

    return uniqueFields;
}

/**
 * Validate if a station has a specific sensor
 * @param {string} stationId - Station ID
 * @param {string} sensor - Sensor name to validate
 * @returns {Promise<boolean>} True if the sensor is active
 */
async function hasSensor(stationId, sensor) {
    try {
        const allowedFields = await getAllowedFields(stationId, "raw");
        const sensorField = SENSOR_FIELD_MAP.raw[sensor];
        const fieldsToCheck = Array.isArray(sensorField)
            ? sensorField
            : [sensorField];
        return fieldsToCheck.some((field) => allowedFields.includes(field));
    } catch {
        return false;
    }
}

/**
 * Middleware to validate requested fields
 * @param {'raw'|'hourly'|'daily'} dataType - Data type
 */
function validateFields(dataType) {
    return async (req, res, next) => {
        try {
            const { station_id, fields } = req.query;

            if (!station_id) return next();

            req.allowedFields = await getAllowedFields(station_id, dataType);

            if (fields) {
                const requestedFields = fields.split(",").map((f) => f.trim());
                const invalidFields = requestedFields.filter(
                    (f) => !req.allowedFields.includes(f),
                );

                if (invalidFields.length > 0) {
                    return res.status(400).json({
                        error: `Invalid fields: ${invalidFields.join(", ")}`,
                        allowedFields: req.allowedFields,
                    });
                }
            }

            next();
        } catch (error) {
            error.status = error.message.includes("not found") ? 404 : 400;
            next(error);
        }
    };
}

module.exports = {
    getStationSensors,
    getAllowedFields,
    validateFields,
    hasSensor, // New exported function
};
