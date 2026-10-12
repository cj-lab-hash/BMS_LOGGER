import express from "express";

const app = express();

app.use(express.json());

app.post("/api/bms", async (req, res) => {

  try {

    const response = await fetch(
      `${process.env.SUPABASE_URL}/rest/v1/bms_logs`,
      {
        method: "POST",
        headers: {
          apikey: process.env.SUPABASE_SERVICE_ROLE_KEY,
          Authorization:
            `Bearer ${process.env.SUPABASE_SERVICE_ROLE_KEY}`,
          "Content-Type": "application/json",
          Prefer: "return=representation"
        },
        body: JSON.stringify(req.body)
      }
    );

    const data = await response.json();

    res.json(data);

  } catch (err) {
    console.error(err);
    res.status(500).json({
      error: err.message
    });
  }
});

app.listen(3000);