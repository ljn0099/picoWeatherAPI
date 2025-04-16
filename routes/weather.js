const express = require("express");
const router = express.Router();
const pool = require("../db");
const { DateTime } = require("luxon");
const {
    validateFields,
    hasSensor,
    getStationSensors,
} = require("../utils/weatherUtils");

/**
 * @swagger
 * tags:
 *   name: Weather
 *   description: Weather data endpoints
 */

// Helper function to format response rows with timezone
function formatResponseRows(rows, timezone) {
    return rows.map((row) => ({
        ...row,
        date: DateTime.fromISO(row.date.toISOString(), {
            zone: timezone,
        }).toISO(),
    }));
}

function parseDateParams({ date, start_date, end_date, timezone }) {
    let queryStart, queryEnd;

    if (date) {
        const isoDate = DateTime.fromISO(date, { zone: timezone });
        if (!isoDate.isValid) throw new Error("Invalid date format");
        queryStart = isoDate.startOf("day");
        queryEnd = isoDate.endOf("day");
    } else {
        queryStart = DateTime.fromISO(start_date, { zone: timezone }).startOf(
            "day",
        );
        queryEnd = DateTime.fromISO(end_date, { zone: timezone }).endOf("day");

        if (!queryStart.isValid || !queryEnd.isValid)
            throw new Error("Invalid date format in start_date or end_date");
        if (queryStart > queryEnd)
            throw new Error("start_date must be before end_date");
    }

    return { queryStart, queryEnd };
}

// Common weather query execution
async function executeWeatherQuery({
    table,
    station_id,
    dateParams,
    timezone,
    fields,
    allowedFields,
}) {
    const { queryStart, queryEnd } = parseDateParams(dateParams);

    const selectedFields = fields
        ? [
              "station_id",
              "date",
              ...fields.split(",").map((f) => f.trim()),
          ].join(", ")
        : allowedFields.join(", ");

    const result = await pool.query(
        `SELECT ${selectedFields} FROM ${table}
    WHERE station_id = $1
    AND date >= $2
    AND date <= $3
    ORDER BY date`,
        [station_id, queryStart.toUTC().toISO(), queryEnd.toUTC().toISO()],
    );

    if (result.rows.length === 0) {
        throw new Error("No data found for the specified period");
    }

    return formatResponseRows(result.rows, timezone);
}
/**
 * @swagger
 * /weather/get-stations:
 *   get:
 *     summary: Get a list of all weather stations
 *     tags: [Weather]
 *     description: Retrieve a list of all weather stations, including their station ID, name, and location.
 *     responses:
 *       200:
 *         description: A list of weather stations
 *         content:
 *           application/json:
 *             schema:
 *               type: object
 *               properties:
 *                 stations:
 *                   type: array
 *                   items:
 *                     type: object
 *                     properties:
 *                       station_id:
 *                         type: integer
 *                         description: The unique ID of the weather station
 *                       name:
 *                         type: string
 *                         description: The name of the weather station
 *                       location:
 *                         type: string
 *                         description: The location of the weather station
 *       404:
 *         description: No stations found
 *       500:
 *         description: Internal server error
 */

router.get("/get-stations", async (req, res) => {
    try {
        // Query the database to get station_id, name, and location of all weather stations
        const { rows } = await pool.query(
            `SELECT station_id, name, location FROM weather_stations`,
        );

        if (rows.length === 0) {
            return res.status(404).json({
                error: "No stations found",
            });
        }

        // Map the results to a cleaner format
        const stations = rows.map((station) => ({
            station_id: station.station_id,
            name: station.name,
            location: station.location,
        }));

        // Send the list of stations as a JSON response
        res.json({ stations });
    } catch (err) {
        // Handle errors and log them
        console.error("Error in /get-stations:", err);
        res.status(500).json({
            error: "Error getting the stations list",
            details: err.message,
        });
    }
});

/**
 * @swagger
 * /weather/station-info:
 *   get:
 *     summary: Get weather station information
 *     tags: [Weather]
 *     parameters:
 *       - in: query
 *         name: station_id
 *         required: true
 *         schema:
 *           type: integer
 *         description: The station ID
 *     responses:
 *       200:
 *         description: Successful response with station info
 *         content:
 *           application/json:
 *             schema:
 *               type: object
 *               properties:
 *                 station_id:
 *                   type: integer
 *                 name:
 *                   type: string
 *                 location:
 *                   type: string
 *                 measure_interval_mins:
 *                   type: integer
 *                   description: Interval in minutes between measurements
 *                 sensors:
 *                   type: object
 *                   properties:
 *                     temperature:
 *                       type: boolean
 *                     humidity:
 *                       type: boolean
 *                     pressure:
 *                       type: boolean
 *                     rain:
 *                       type: boolean
 *                     wind_speed:
 *                       type: boolean
 *                     wind_direction:
 *                       type: boolean
 *                     gust_speed:
 *                       type: boolean
 *                     gust_direction:
 *                       type: boolean
 *                     lux:
 *                       type: boolean
 *                     uv_index:
 *                       type: boolean
 *       400:
 *         description: Invalid request
 *         content:
 *           application/json:
 *             schema:
 *               type: object
 *               properties:
 *                 error:
 *                   type: string
 *                 details:
 *                   type: string
 */

router.get("/station-info", async (req, res) => {
    try {
        const { station_id } = req.query;

        if (!station_id) {
            return res.status(400).json({
                error: "station_id parameter is required",
                example: "/weather/station-info?station_id=1",
            });
        }

        const { rows } = await pool.query(
            `SELECT
          name,
          location,
          temperature,
          humidity,
          pressure,
          rain_gauge,
          anemometer,
          wind_vane,
          lux,
          uvi,
          measure_interval_mins
       FROM weather_stations
       WHERE station_id = $1`,
            [station_id],
        );

        if (rows.length === 0) {
            return res.status(404).json({
                error: "Station not found",
                station_id,
            });
        }

        const stationData = rows[0];
        const response = {
            station_id,
            name: stationData.name,
            location: stationData.location,
            measure_interval_mins: stationData.measure_interval_mins,
            sensors: {
                temperature: stationData.temperature,
                humidity: stationData.humidity,
                pressure: stationData.pressure,
                rain: stationData.rain_gauge,
                wind_speed: stationData.anemometer,
                wind_direction: stationData.wind_vane,
                gust_speed: stationData.anemometer,
                gust_direction: stationData.anemometer && stationData.wind_vane,
                lux: stationData.lux,
                uvi: stationData.uvi,
            },
        };

        res.json(response);
    } catch (err) {
        console.error("Error in /station-info:", err);
        res.status(500).json({
            error: "Error getting the station information",
            details: err.message,
        });
    }
});

/**
 * @swagger
 * /weather/raw-data:
 *   get:
 *     summary: Get raw weather measurements
 *     tags: [Weather]
 *     parameters:
 *       - in: query
 *         name: station_id
 *         required: true
 *         schema:
 *           type: integer
 *       - in: query
 *         name: timezone
 *         required: true
 *         schema:
 *           type: string
 *       - in: query
 *         name: date
 *         schema:
 *           type: string
 *           format: date
 *       - in: query
 *         name: start_date
 *         schema:
 *           type: string
 *           format: date
 *       - in: query
 *         name: end_date
 *         schema:
 *           type: string
 *           format: date
 *       - in: query
 *         name: fields
 *         schema:
 *           type: string
 *     responses:
 *       200:
 *         description: Successful response with raw data
 *         content:
 *           application/json:
 *             schema:
 *               type: array
 *               items:
 *                 type: object
 *                 properties:
 *                   id:
 *                     type: integer
 *                   station_id:
 *                     type: integer
 *                   date:
 *                     type: string
 *                     format: date-time
 *                   temperature:
 *                     type: number
 *                   humidity:
 *                     type: number
 *                   pressure:
 *                     type: number
 *                   rain:
 *                     type: number
 *                   wind_speed:
 *                     type: number
 *                   gust_speed:
 *                     type: number
 *                   wind_direction:
 *                     type: number
 *                   gust_direction:
 *                     type: number
 *                   lux:
 *                     type: integer
 *                   uvi:
 *                     type: number
 *       400:
 *         description: Invalid parameters
 *         content:
 *           application/json:
 *             schema:
 *               type: object
 *               properties:
 *                 error:
 *                   type: string
 *                 suggestion:
 *                   type: string
 */
router.get("/raw-data", validateFields("raw"), async (req, res) => {
    try {
        const { station_id, date, start_date, end_date, timezone, fields } =
            req.query;

        if (!station_id || !timezone || (!date && (!start_date || !end_date))) {
            throw new Error(
                "Missing required parameters: station_id and timezone are always required, plus either date or both start_date and end_date",
            );
        }

        const data = await executeWeatherQuery({
            table: "weather_data",
            station_id,
            dateParams: { date, start_date, end_date, timezone },
            timezone,
            fields,
            allowedFields: req.allowedFields,
        });

        res.json(data);
    } catch (err) {
        console.error(err);
        res.status(400).json({ error: err.message });
    }
});

/**
 * @swagger
 * /weather/hourly-summary:
 *   get:
 *     summary: Get hourly aggregated data
 *     tags: [Weather]
 *     parameters:
 *       - in: query
 *         name: station_id
 *         required: true
 *         schema:
 *           type: integer
 *       - in: query
 *         name: timezone
 *         required: true
 *         schema:
 *           type: string
 *       - in: query
 *         name: date
 *         schema:
 *           type: string
 *           format: date
 *       - in: query
 *         name: start_date
 *         schema:
 *           type: string
 *           format: date
 *       - in: query
 *         name: end_date
 *         schema:
 *           type: string
 *           format: date
 *       - in: query
 *         name: fields
 *         schema:
 *           type: string
 *     responses:
 *       200:
 *         description: Successful response with hourly data
 *         content:
 *           application/json:
 *             schema:
 *               type: array
 *               items:
 *                 type: object
 *                 properties:
 *                   id:
 *                     type: integer
 *                   station_id:
 *                     type: integer
 *                   date:
 *                     type: string
 *                     format: date-time
 *                   avg_temperature:
 *                     type: number
 *                   avg_humidity:
 *                     type: number
 *                   avg_pressure:
 *                     type: number
 *                   sum_rain:
 *                     type: number
 *                   avg_wind_speed:
 *                     type: number
 *                   max_gust_speed:
 *                     type: number
 *                   avg_wind_direction:
 *                     type: number
 *                   max_gust_direction:
 *                     type: number
 *                   avg_lux:
 *                     type: integer
 *                   avg_uvi:
 *                     type: number
 *       400:
 *         description: Invalid parameters
 */
router.get("/hourly-summary", validateFields("hourly"), async (req, res) => {
    try {
        const { station_id, date, start_date, end_date, timezone, fields } =
            req.query;

        if (!station_id || !timezone || (!date && (!start_date || !end_date))) {
            throw new Error(
                "Missing required parameters: station_id and timezone are always required, plus either date or both start_date and end_date",
            );
        }

        const data = await executeWeatherQuery({
            table: "weather_hourly",
            station_id,
            dateParams: { date, start_date, end_date, timezone },
            timezone,
            fields,
            allowedFields: req.allowedFields,
        });

        res.json(data);
    } catch (err) {
        console.error(err);
        res.status(400).json({ error: err.message });
    }
});

/**
 * @swagger
 * /weather/daily-summary:
 *   get:
 *     summary: Get daily aggregated data (optimized for timezones matching Europe/Madrid)
 *     tags: [Weather]
 *     parameters:
 *       - in: query
 *         name: station_id
 *         required: true
 *         schema:
 *           type: integer
 *       - in: query
 *         name: timezone
 *         required: true
 *         schema:
 *           type: string
 *       - in: query
 *         name: date
 *         schema:
 *           type: string
 *           format: date
 *       - in: query
 *         name: start_date
 *         schema:
 *           type: string
 *           format: date
 *       - in: query
 *         name: end_date
 *         schema:
 *           type: string
 *           format: date
 *       - in: query
 *         name: fields
 *         schema:
 *           type: string
 *     responses:
 *       200:
 *         description: Successful response with daily data
 *         content:
 *           application/json:
 *             schema:
 *               type: array
 *               items:
 *                 type: object
 *                 properties:
 *                   id:
 *                     type: integer
 *                   station_id:
 *                     type: integer
 *                   date:
 *                     type: string
 *                     format: date
 *                   max_temperature:
 *                     type: number
 *                   min_temperature:
 *                     type: number
 *                   max_humidity:
 *                     type: number
 *                   min_humidity:
 *                     type: number
 *                   max_pressure:
 *                     type: number
 *                   min_pressure:
 *                     type: number
 *                   avg_wind_speed:
 *                     type: number
 *                   max_gust_speed:
 *                     type: number
 *                   avg_wind_direction:
 *                     type: number
 *                   max_gust_direction:
 *                     type: number
 *                   max_uvi:
 *                     type: number
 *                   max_lux:
 *                     type: integer
 *                   min_lux:
 *                     type: integer
 *                   sum_rain:
 *                     type: number
 *       400:
 *         description: Invalid parameters
 */
router.get("/daily-summary", validateFields("daily"), async (req, res) => {
    try {
        // Extract and validate parameters
        const { station_id, date, start_date, end_date, timezone, fields } =
            req.query;

        if (!station_id) throw new Error("station_id is required");
        if (!timezone) throw new Error("timezone is required");
        if (!DateTime.now().setZone(timezone).isValid)
            throw new Error("Invalid timezone");
        if (!date && (!start_date || !end_date)) {
            throw new Error(
                "Either date or both start_date and end_date are required",
            );
        }

        // Parse date range
        const { queryStart, queryEnd } = parseDateParams({
            date,
            start_date,
            end_date,
            timezone,
        });

        // Get allowed fields for this station (already filtered by station sensors)
        // The validateFields middleware already populated req.allowedFields

        // Determine which fields to include in the response
        const selectedFields = fields
            ? ["station_id", "date", ...fields.split(",").map((f) => f.trim())]
            : req.allowedFields;

        // Check if timezone is compatible with the database table
        const isCompatible = await isCompatibleTimezone(
            timezone,
            queryStart,
            queryEnd,
        );

        let result;

        // If timezone is compatible with the database table, use direct query
        if (isCompatible) {
            // Use existing records from the database table
            result = await pool.query(
                `SELECT ${selectedFields.join(", ")}
                FROM weather_daily
                WHERE station_id = $1
                AND date >= $2
                AND date <= $3
                ORDER BY date`,
                [station_id, queryStart.toSQLDate(), queryEnd.toSQLDate()],
            );
        } else {
            // For incompatible timezones, calculate on the fly using the provided query
            result = await pool.query(
                `WITH date_range AS (
                    SELECT
                        generate_series(
                            date_trunc('day', $2::timestamptz AT TIME ZONE $3),
                            date_trunc('day', $4::timestamptz AT TIME ZONE $3),
                            interval '1 day'
                        ) AS local_day
                ),
                day_ranges AS (
                    SELECT
                        local_day AS local_day_start,
                        local_day + interval '1 day' - interval '1 second' AS local_day_end,
                        local_day::date AS local_date
                    FROM date_range
                ),
                utc_ranges AS (
                    SELECT
                        local_day_start,
                        local_day_end,
                        local_day_start AT TIME ZONE $3 AT TIME ZONE 'UTC' AS utc_day_start,
                        local_day_end AT TIME ZONE $3 AT TIME ZONE 'UTC' AS utc_day_end,
                        local_date
                    FROM day_ranges
                ),
                filtered_data AS (
                    SELECT 
                        wd.*,
                        ur.local_date
                    FROM weather_data wd
                    JOIN utc_ranges ur ON 
                        wd.date >= ur.utc_day_start AND 
                        wd.date <= ur.utc_day_end
                    WHERE wd.station_id = $1
                ),
                daily_max_gust AS (
                    SELECT 
                        local_date,
                        gust_speed, 
                        gust_direction
                    FROM (
                        SELECT 
                            local_date,
                            gust_speed, 
                            gust_direction,
                            ROW_NUMBER() OVER (PARTITION BY local_date ORDER BY gust_speed DESC) as rn
                        FROM filtered_data
                        WHERE gust_speed IS NOT NULL
                    ) t
                    WHERE rn = 1
                )
                SELECT
                    $1::integer AS station_id,
                    fd.local_date AS date,
                    MAX(fd.temperature) FILTER (WHERE fd.temperature IS NOT NULL) AS max_temperature,
                    MIN(fd.temperature) FILTER (WHERE fd.temperature IS NOT NULL) AS min_temperature,
                    MAX(fd.humidity) FILTER (WHERE fd.humidity IS NOT NULL) AS max_humidity,
                    MIN(fd.humidity) FILTER (WHERE fd.humidity IS NOT NULL) AS min_humidity,
                    MAX(fd.pressure) FILTER (WHERE fd.pressure IS NOT NULL) AS max_pressure,
                    MIN(fd.pressure) FILTER (WHERE fd.pressure IS NOT NULL) AS min_pressure,
                    MAX(dmg.gust_speed) AS max_gust_speed,
                    MAX(dmg.gust_direction) AS max_gust_direction,
                    CASE WHEN COUNT(fd.wind_speed) > 0 THEN STDDEV_POP(fd.wind_speed) ELSE NULL END AS standard_deviation_speed,
                    CASE WHEN COUNT(fd.wind_speed) > 0 THEN AVG(fd.wind_speed) ELSE NULL END AS avg_wind_speed,
                    CASE WHEN COUNT(fd.wind_direction) > 0 THEN
                        MOD(
                            CAST(DEGREES(ATAN2(SUM(SIN(RADIANS(fd.wind_direction))), SUM(COS(RADIANS(fd.wind_direction))))) + 360.0 AS numeric),
                            CAST(360.0 AS numeric)
                        )
                    ELSE NULL END AS avg_wind_direction,
                    MAX(fd.uvi) FILTER (WHERE fd.uvi IS NOT NULL) AS max_uvi,
                    MAX(fd.lux) FILTER (WHERE fd.lux IS NOT NULL) AS max_lux,
                    MIN(fd.lux) FILTER (WHERE fd.lux IS NOT NULL) AS min_lux,
                    SUM(fd.rain) FILTER (WHERE fd.rain IS NOT NULL) AS sum_rain
                FROM filtered_data fd
                LEFT JOIN daily_max_gust dmg ON fd.local_date = dmg.local_date
                GROUP BY fd.local_date
                ORDER BY fd.local_date`,
                [station_id, queryStart.toISO(), timezone, queryEnd.toISO()],
            );
        }

        // Format the results to include only the selected fields and proper date formatting
        const formattedData = result.rows.map((row) => {
            // Create a base object with the date and station ID
            const baseObj = {
                station_id: parseInt(station_id),
                date: DateTime.fromJSDate(row.date, { setZone: true }).toISODate()
            };

            // Add each field that was requested
            return selectedFields.reduce((obj, field) => {
                // Skip station_id and date as they're already included
                if (
                    field !== "station_id" &&
                    field !== "date" &&
                    row[field] !== undefined
                ) {
                    obj[field] = row[field];
                }
                return obj;
            }, baseObj);
        });

        if (formattedData.length === 0) {
            throw new Error("No data found for the specified criteria");
        }

        res.json(formattedData);
    } catch (error) {
        console.error("Daily summary error:", error);
        res.status(400).json({
            error: error.message,
            details: error.details || null,
        });
    }
});

/**
 * Checks if a timezone has the same UTC offset as Madrid for all dates in range
 */
async function isCompatibleTimezone(tz, startDate, endDate) {
    const days = endDate.diff(startDate, "days").days;
    for (let i = 0; i <= days; i++) {
        const currentDate = startDate.plus({ days: i });
        const madridOffset = currentDate.setZone("Europe/Madrid").offset;
        const tzOffset = currentDate.setZone(tz).offset;
        if (madridOffset !== tzOffset) return false;
    }
    return true;
}
/**
 * @swagger
 * /weather/rainfall-last-mins:
 *   get:
 *     summary: Get rainfall accumulation
 *     tags: [Weather]
 *     parameters:
 *       - in: query
 *         name: station_id
 *         required: true
 *         schema:
 *           type: integer
 *       - in: query
 *         name: date
 *         required: true
 *         schema:
 *           type: string
 *           format: date-time
 *       - in: query
 *         name: timezone
 *         required: true
 *         schema:
 *           type: string
 *       - in: query
 *         name: mins
 *         required: true
 *         schema:
 *           type: integer
 *           minimum: 1
 *     responses:
 *       200:
 *         description: Successful rainfall calculation
 *         content:
 *           application/json:
 *             schema:
 *               type: object
 *               properties:
 *                 station_id:
 *                   type: integer
 *                 start_time:
 *                   type: string
 *                   format: date-time
 *                 end_time:
 *                   type: string
 *                   format: date-time
 *                 minutes:
 *                   type: integer
 *                 rainfall:
 *                   type: number
 *       400:
 *         description: Invalid parameters
 *       404:
 *         description: Station has no rain gauge
 */
router.get("/rainfall-last-mins", async (req, res) => {
    try {
        const { station_id, date, timezone, mins } = req.query;

        if (!station_id || !date || !timezone || !mins) {
            throw new Error(
                "Missing required parameters: station_id, date, timezone, mins",
            );
        }

        // Validate rain sensor
        const hasRain = await hasSensor(station_id, "rain_gauge");
        if (!hasRain) {
            return res.status(404).json({
                error: "Station has no rain gauge",
                station_id,
                available_sensors: await getStationSensors(station_id),
            });
        }

        // Validate minutes
        const minutes = parseInt(mins, 10);
        if (isNaN(minutes)) {
            throw new Error("Invalid minutes value");
        }

        // Validate date
        const dateTime = DateTime.fromISO(date, { zone: timezone });
        if (!dateTime.isValid) {
            throw new Error("Invalid date format");
        }

        // Query database
        const { rows } = await pool.query(
            `SELECT COALESCE(SUM(rain), 0) AS rainfall
         FROM weather_data
         WHERE station_id = $1
         AND date BETWEEN $2 AND $3`,
            [
                station_id,
                dateTime.minus({ minutes }).toUTC().toISO(),
                dateTime.toUTC().toISO(),
            ],
        );

        res.json({
            station_id,
            start_time: dateTime.minus({ minutes }).toISO(),
            end_time: dateTime.toISO(),
            minutes,
            rainfall: rows[0].rainfall,
        });
    } catch (err) {
        console.error(err);
        const status = err.message.includes("no tiene") ? 404 : 400;
        res.status(status).json({
            error: err.message,
            details: err.details || null,
        });
    }
});

module.exports = router;
