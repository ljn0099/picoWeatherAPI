# -----------------------------------------------
# Stage 1: Builder (install dependencies and build)
# -----------------------------------------------
FROM node:23-alpine AS builder

# Create and set working directory
WORKDIR /usr/src/picoWeatherAPI

# Copy package files first for better layer caching
COPY package*.json ./

# Install production dependencies (clean cache afterwards)
RUN npm ci --only=production && \
    npm cache clean --force

# Copy application source
COPY . .

# -----------------------------------------------
# Stage 2: Runtime (production-optimized image)
# -----------------------------------------------
FROM node:23-alpine

# Install dumb-init for proper signal handling
RUN apk add --no-cache dumb-init && \
    # Create non-root user for security
    adduser -D weatherApi && \
    # Create app directory and set permissions
    mkdir -p /usr/src/picoWeatherAPI && \
    chown weatherApi:weatherApi /usr/src/picoWeatherAPI

WORKDIR /usr/src/picoWeatherAPI

# Copy from builder stage
COPY --from=builder --chown=weatherApi:weatherApi /usr/src/picoWeatherAPI .

# Environment variables (non-sensitive config only)
ENV NODE_ENV=production \
    PORT=5000

# Expose application port
EXPOSE ${PORT}

# Switch to non-root user
USER weatherApi

# Health check (verify app is responsive)
HEALTHCHECK --interval=30s --timeout=3s \
  CMD curl -f http://localhost:5000/health || exit 1

# Entrypoint for proper signal handling
ENTRYPOINT ["dumb-init", "--"]

# Start command (direct node execution for production)
CMD ["node", "server.js"]
