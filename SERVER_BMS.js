
import express from "express";



const app = express();



app.use(express.json());


app.get("/", (req,res) => {
  res.json({
    status:"online"
  });
});

async function sendTelegramMessage(message) {
    try {

        const token = process.env.TELEGRAM_BOT_TOKEN;
        const chatId = process.env.TELEGRAM_ID;

        const response = await fetch(
            `https://api.telegram.org/bot${token}/sendMessage`,
            {
                method: "POST",
                headers: {
                    "Content-Type": "application/json"
                },
                body: JSON.stringify({
                    chat_id: chatId,
                    text: message
                })
            }
        );


    } catch (err) {
        console.error("Telegram Error:", err);
    }
}

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
  const lastReportedSoc = await getLastReportedSoc();
  if(lastReportedSoc === null) {
    await sendTelegramMessage(
      `🔋 Battery Monitoring Started
      SOC: ${soc}%
      Voltage: ${voltage}V
      Current: ${current}A
      `
    );
    await setLastReportedSoc(soc);
  } else if (Math.abs(soc - lastReportedSoc) >= 10) {
    const direction = soc > lastReportedSoc
      ? "Charging ⬆️"
      : "Discharging ⬇️";
      await sendTelegramMessage(
        `🔋 Battery Update
        ${direction}
        SOC: ${soc}%
        Voltage: ${voltage}V
        Current: ${current}A
        Temperature: ${temperature}°C
        Cycles: ${cycles}
        Change: ${soc - lastReportedSoc}%
        `
      );
      await setLastReportedSoc(soc);
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

async function getLastReportedSoc() {

  const response = await fetch(
    `${process.env.SUPABASE_URL}/rest/v1/system_settings?setting_key=eq.last_reported_soc&select=setting_value`,
    {
      headers: {
        apikey: process.env.SUPABASE_SERVICE_ROLE_KEY,
        Authorization: `Bearer ${process.env.SUPABASE_SERVICE_ROLE_KEY}`
      }
    }
  );

  const data = await response.json();

  if (!data?.length || data[0].setting_value === null) {
    return null;
  }

  return Number(data[0].setting_value);
}

async function setLastReportedSoc(value) {

  await fetch(
    `${process.env.SUPABASE_URL}/rest/v1/system_settings?setting_key=eq.last_reported_soc`,
    {
      method: "PATCH",
      headers: {
        apikey: process.env.SUPABASE_SERVICE_ROLE_KEY,
        Authorization: `Bearer ${process.env.SUPABASE_SERVICE_ROLE_KEY}`,
        "Content-Type": "application/json"
      },
      body: JSON.stringify({
        setting_value: String(value),
        updated_at: new Date().toISOString()
      })
    }
  );
}

const PORT = process.env.PORT || 3000;

app.listen(PORT, () => {
  console.log(`Running on ${PORT}`);
});
