import express from "express";

const app = express();

app.use(express.json());


app.get("/", (req,res) => {
  res.json({
    status:"online"
  });
});

app.post("/api/bms", async (req, res) => {
  

  try {

      const apiKey = req.headers["x-device-key"];
  if(apiKey!==process.env.ESP32_DEVICE_KEY){
    return res.status(401).json({error:"Unauthorized"});
  }

  const {
    device_name,
    voltage,
    current,
    soc,
    temperature,
    cycles
  } = req.body;
  
  if(device_name === undefined ||
    voltage === undefined ||
    current === undefined ||
    soc === undefined
  ) {
    return res.status(400).json({
      error: "Missing required fields"
    });
  }
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
    if(!response.ok){
      console.error("Supabase Error:", data);
      return res.status(response.status).json({
        error: "Supabase insert failed",
        details: data
      });
    }
    res.json(data);

  } catch (err) {
    console.error(err);
    res.status(500).json({
      error: err.message
    });
  }
});
const PORT = process.env.PORT || 3000;

app.listen(PORT, () => {
  console.log(`Running on ${PORT}`);
});
