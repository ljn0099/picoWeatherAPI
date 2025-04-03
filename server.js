const express = require("express");
const cors = require("cors");
const helmet = require("helmet");
const rateLimit = require("express-rate-limit");
const weatherRoutes = require("./routes/weather");
const setupDocs = require("./routes/docs");

const app = express();

// Security Middleware
app.use(helmet());
app.use(cors());
app.use(express.json());

// Rate Limiting (100 requests per 15 minutes)
const limiter = rateLimit({
  windowMs: 15 * 60 * 1000, // 15 minutes
  max: 100
});
app.use(limiter);

// Routes
app.use("/weather", weatherRoutes);

// Health Check Endpoint
app.get("/health", (req, res) => {
  res.status(200).json({ status: "healthy" });
});

// API Documentation
setupDocs(app);

// Error Handling Middleware
app.use((err, req, res, next) => {
  console.error(err.stack);
  res.status(500).json({
    error: "Internal Server Error",
    message: err.message
  });
});

const PORT = parseInt(process.env.PORT, 10) || 5000;
const ENVIRONMENT = process.env.NODE_ENV || "development";

app.listen(PORT, () => {
  console.log(`Server running in ${ENVIRONMENT} mode`);
  console.log(`API running at http://localhost:${PORT}`);
  console.log(`API docs at http://localhost:${PORT}/api-docs`);
  console.log(`Health check at http://localhost:${PORT}/health`);
});
