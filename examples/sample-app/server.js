import express from "express";

const app = express();
const port = process.env.PORT || 8080;

app.get("/", (_request, response) => {
  response.json({
    service: "orbitdesktop-sample-app",
    status: "running",
    environment: process.env.ORBIT_ENVIRONMENT_ID || "local",
    databaseConfigured: Boolean(process.env.DATABASE_URL)
  });
});

app.listen(port, () => {
  console.log(`sample app listening on ${port}`);
});
