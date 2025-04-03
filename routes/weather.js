const express = require("express");
const router = express.Router();
const pool = require("../db");
const { DateTime } = require("luxon");
const { validateFields, hasSensor, getStationSensors } = require("../utils/weatherUtils");

/**
 * @swagger
 * tags:
 *   name: Weather
 *   description: Weather data endpoints
 */

// Helper function to format response rows with timezone
function formatResponseRows(rows, timezone) {
  return rows.map(row => ({
    ...row,
    date: DateTime.fromISO(row.date.toISOString(), { zone: timezone }).toISO()
  }));
}

// Helper function to parse date parameters
function parseDateParams({ date, start_date, end_date, timezone }) {
  let queryStart, queryEnd;

  if (date) {
    const isoDate = DateTime.fromISO(date, { zone: timezone });
    if (!isoDate.isValid) throw new Error("Invalid date format");
    queryStart = isoDate.startOf("day");
    queryEnd = isoDate.endOf("day");
  } else {
    queryStart = DateTime.fromISO(start_date, { zone: timezone });
    queryEnd = DateTime.fromISO(end_date, { zone: timezone });

    if (!queryStart.isValid || !queryEnd.isValid) throw new Error("Invalid date format in start_date or end_date");
    if (queryStart > queryEnd) throw new Error("start_date must be before end_date");
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
  allowedFields
}) {
  const { queryStart, queryEnd } = parseDateParams(dateParams);

  const selectedFields = fields ?
    ['station_id', 'date', ...fields.split(',').map(f => f.trim())].join(', ') :
    allowedFields.join(', ');

  const result = await pool.query(
    `SELECT ${selectedFields} FROM ${table}
    WHERE station_id = $1
    AND date >= $2
    AND date <= $3
    ORDER BY date`,
    [station_id, queryStart.toUTC().toISO(), queryEnd.toUTC().toISO()]
  );

  if (result.rows.length === 0) {
    throw new Error("No data found for the specified period");
  }

  return formatResponseRows(result.rows, timezone);
}

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
 *                     light:
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
        example: "/weather/station-info?station_id=1"
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
          uvi
       FROM weather_stations
       WHERE station_id = $1`,
      [station_id]
    );

    if (rows.length === 0) {
      return res.status(404).json({
        error: "Station not found",
        station_id
      });
    }

    const stationData = rows[0];
    const response = {
      station_id,
      name: stationData.name,
      location: stationData.location,
      sensors: {
        temperature: stationData.temperature,
        humidity: stationData.humidity,
        pressure: stationData.pressure,
        rain: stationData.rain_gauge,
        wind_speed: stationData.anemometer,
        wind_direction: stationData.wind_vane,
        light: stationData.lux,
        uv_index: stationData.uvi
      },
    };

    res.json(response);
  } catch (err) {
    console.error('Error in /station-info:', err);
    res.status(500).json({
      error: "Error getting the station information",
      details: err.message
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
 *                   average_speed:
 *                     type: number
 *                   peak_speed:
 *                     type: number
 *                   direction:
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
router.get("/raw-data",
  validateFields('raw'),
  async (req, res) => {
    try {
      const { station_id, date, start_date, end_date, timezone, fields } = req.query;

      if (!station_id || !timezone || (!date && (!start_date || !end_date))) {
        throw new Error("Missing required parameters: station_id and timezone are always required, plus either date or both start_date and end_date");
      }

      const data = await executeWeatherQuery({
        table: 'weather_data',
        station_id,
        dateParams: { date, start_date, end_date, timezone },
        timezone,
        fields,
        allowedFields: req.allowedFields
      });

      res.json(data);
    } catch (err) {
      console.error(err);
      res.status(400).json({ error: err.message });
    }
  }
);

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
 *                   avg_speed:
 *                     type: number
 *                   max_peak_speed:
 *                     type: number
 *                   mode_direction:
 *                     type: number
 *                   avg_lux:
 *                     type: integer
 *                   avg_uvi:
 *                     type: number
 *       400:
 *         description: Invalid parameters
 */
router.get("/hourly-summary",
  validateFields('hourly'),
  async (req, res) => {
    try {
      const { station_id, date, start_date, end_date, timezone, fields } = req.query;

      if (!station_id || !timezone || (!date && (!start_date || !end_date))) {
        throw new Error("Missing required parameters: station_id and timezone are always required, plus either date or both start_date and end_date");
      }

      const data = await executeWeatherQuery({
        table: 'weather_hourly',
        station_id,
        dateParams: { date, start_date, end_date, timezone },
        timezone,
        fields,
        allowedFields: req.allowedFields
      });

      res.json(data);
    } catch (err) {
      console.error(err);
      res.status(400).json({ error: err.message });
    }
  }
);

/**
 * @swagger
 * /weather/daily-summary:
 *   get:
 *     summary: Get daily aggregated data (only for timezones matching Europe/Madrid)
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
 *                   max_peak_speed:
 *                     type: number
 *                   mode_direction:
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
router.get("/daily-summary",
  validateFields('daily'),
  async (req, res) => {
    try {
      const { station_id, date, start_date, end_date, timezone, fields } = req.query;

      if (!station_id || !timezone || (!date && (!start_date || !end_date))) {
        throw new Error("Missing required parameters: station_id and timezone are always required, plus either date or both start_date and end_date");
      }

      // List of timezones that follow exactly the same DST rules as Europe/Madrid
      const allowedTimezones = [
        'Europe/Madrid',
        'Europe/Paris',
        'Europe/Berlin',
        'Europe/Rome',
        'Europe/Amsterdam',
        'Europe/Brussels',
        'Europe/Vienna',
        'Europe/Prague',
        'Europe/Warsaw',
        'Europe/Copenhagen',
        'Europe/Monaco',
        'Europe/Luxembourg',
        'Europe/Andorra',
        'Europe/Gibraltar',
        'Africa/Ceuta'
      ];

      if (!allowedTimezones.includes(timezone)) {
        throw new Error(`Timezone ${timezone} is not supported. Only timezones that follow the same DST rules as Europe/Madrid are allowed.`);
      }

      // Convert station_id to number
      const stationIdNum = parseInt(station_id, 10);
      if (isNaN(stationIdNum)) {
        throw new Error("station_id must be a number");
      }

      // Parse date parameters
      const { queryStart, queryEnd } = parseDateParams({ date, start_date, end_date, timezone });

      const selectedFields = fields ?
        ['station_id', 'date', ...fields.split(',').map(f => f.trim())].join(', ') :
        req.allowedFields.join(', ');

      const result = await pool.query(
        `SELECT ${selectedFields} FROM weather_daily
         WHERE station_id = $1::integer
         AND date >= $2::date
         AND date <= $3::date
         ORDER BY date`,
        [stationIdNum, queryStart.toFormat('yyyy-MM-dd'), queryEnd.toFormat('yyyy-MM-dd')]
      );

      if (result.rows.length === 0) {
        throw new Error("No data found for the specified period");
      }

      res.json(result.rows.map(row => ({
        ...row,
        date: DateTime.fromSQL(row.date).setZone(timezone).toISODate()
      })));
    } catch (err) {
      console.error(err);
      res.status(400).json({ error: err.message });
    }
  }
);

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
router.get("/rainfall-last-mins",
  async (req, res) => {
    try {
      const { station_id, date, timezone, mins } = req.query;

      if (!station_id || !date || !timezone || !mins) {
        throw new Error("Missing required parameters: station_id, date, timezone, mins");
      }

      // Validate rain sensor
      const hasRain = await hasSensor(station_id, 'rain_gauge');
      if (!hasRain) {
        return res.status(404).json({
          error: "Station has no rain gauge",
          station_id,
          available_sensors: await getStationSensors(station_id)
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
          dateTime.toUTC().toISO()
        ]
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
      const status = err.message.includes('no tiene') ? 404 : 400;
      res.status(status).json({
        error: err.message,
        details: err.details || null
      });
    }
  }
);

module.exports = router;
