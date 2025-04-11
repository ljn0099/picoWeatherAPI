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

        // Build field selection
        const selectedFields = fields
            ? [
                  "station_id",
                  "date",
                  ...new Set(fields.split(",").map((f) => f.trim())),
              ]
            : req.allowedFields;

        // Execute appropriate query
        let result;
        if (await isCompatibleTimezone(timezone, queryStart, queryEnd)) {
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
            result = await pool.query(buildDynamicQuery(selectedFields), [
                station_id,
                queryStart.toISO(),
                queryEnd.toISO(),
                timezone,
            ]);
        }

        // Format response with correct timezone
        const formattedData = result.rows.map((row) => {
            const dateObj = DateTime.fromJSDate(row.date, { zone: "UTC" })
                .setZone(timezone)
                .startOf("day");

            return {
                ...row,
                date: dateObj.toISO(),
            };
        });

        // Filter to exact requested dates
        const filteredData = formattedData.filter((entry) => {
            const entryDate = DateTime.fromISO(entry.date);
            return entryDate >= queryStart && entryDate <= queryEnd;
        });

        if (filteredData.length === 0) {
            throw new Error("No data found for the specified criteria");
        }

        res.json(filteredData);
    } catch (error) {
        console.error("Daily summary error:", error);
        res.status(400).json({
            error: error.message,
            details: error.details || null,
        });
    }
});

/**
 * Builds dynamic query for incompatible timezones
 */
function buildDynamicQuery(fields) {
    const fieldMap = {
        max_temperature: "MAX(temperature) AS max_temperature",
        min_temperature: "MIN(temperature) AS min_temperature",
        max_humidity: "MAX(humidity) AS max_humidity",
        min_humidity: "MIN(humidity) AS min_humidity",
        max_pressure: "MAX(pressure) AS max_pressure",
        min_pressure: "MIN(pressure) AS min_pressure",
        max_gust_speed: "MAX(gust_speed) AS max_gust_speed",
        max_gust_direction:
            "MODE() WITHIN GROUP (ORDER BY gust_direction) AS max_gust_direction",
        avg_wind_speed: "AVG(wind_speed) AS avg_wind_speed",
        avg_wind_direction: `(
      WITH wind AS (
        SELECT AVG(SIN(RADIANS(wind_direction))) AS avg_sin,
               AVG(COS(RADIANS(wind_direction))) AS avg_cos
        FROM data
      )
      SELECT CASE
        WHEN avg_sin IS NOT NULL AND avg_cos IS NOT NULL
        THEN (360 + DEGREES(ATAN2(avg_sin, avg_cos)))::NUMERIC % 360.0
        ELSE NULL
      END FROM wind
    ) AS avg_wind_direction`,
        standard_deviation_speed:
            "STDDEV(wind_speed) AS standard_deviation_speed",
        max_uvi: "MAX(uvi) AS max_uvi",
        max_lux: "MAX(lux) AS max_lux",
        min_lux: "MIN(lux) AS min_lux",
        sum_rain: "SUM(rain) AS sum_rain",
    };

    const selectedFields = fields
        .filter((f) => f !== "station_id" && f !== "date")
        .map((f) => fieldMap[f] || "")
        .filter(Boolean);

    return `
    WITH data AS (
      SELECT
        station_id,
        (date AT TIME ZONE 'UTC' AT TIME ZONE $4)::date AS local_date,
        temperature,
        humidity,
        pressure,
        wind_speed,
        wind_direction,
        gust_speed,
        gust_direction,
        uvi,
        lux,
        rain
      FROM weather_data
      WHERE station_id = $1
        AND date >= $2
        AND date <= $3
    )
    SELECT
      station_id,
      local_date AS date,
      ${selectedFields.join(",\n      ")}
    FROM data
    GROUP BY station_id, local_date
    ORDER BY local_date`;
}

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
