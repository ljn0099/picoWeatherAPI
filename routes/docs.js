const swaggerJsdoc = require("swagger-jsdoc");
const swaggerUi = require("swagger-ui-express");
const path = require("path");

const options = {
    definition: {
        openapi: "3.0.0",
        info: {
            title: "Weather API",
            version: "1.0.0",
            description: "API for weather station data",
            license: {
                name: "CC-BY 4.0",
                url: "https://creativecommons.org/licenses/by/4.0/",
            },
        },
    },
    apis: [path.join(__dirname, "./weather.js")],
};

const specs = swaggerJsdoc(options);

module.exports = (app) => {
    app.use(
        "/",
        swaggerUi.serve,
        swaggerUi.setup(specs, {
            customSiteTitle: "Weather API Docs",
            customCss: ".swagger-ui .topbar { display: none }",
        }),
    );

    app.get("/.json", (req, res) => {
        res.setHeader("Content-Type", "application/json");
        res.send(specs);
    });
};
