from fastapi import FastAPI, Query
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from pymongo import MongoClient
from datetime import datetime, timezone, timedelta
import re
import os
from dotenv import load_dotenv

load_dotenv()

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

client = MongoClient(os.getenv("MONGO_URI"))
db = client["smart_hub"]
settings_collection = db["settings"]
data_collection = db["sensor_data"]

# Jamaica is UTC-5
JAMAICA_TZ = timezone(timedelta(hours=-5))


# ── Models ────────────────────────────────────────────────────────────────────

class SettingsIn(BaseModel):
    user_temp: float
    user_light: str        # "HH:MM:SS"
    light_duration: str    # e.g. "4h", "2h30m", "45m"


class SensorData(BaseModel):
    temperature: float
    presence: bool


# ── Helpers ───────────────────────────────────────────────────────────────────

def parse_duration_to_minutes(duration: str) -> int:
    pattern = r"(?:(\d+)h)?(?:(\d+)m)?"
    match = re.fullmatch(pattern, duration.strip())
    if not match or not match.group(0):
        raise ValueError(f"Invalid light_duration format: {duration!r}")
    hours = int(match.group(1) or 0)
    minutes = int(match.group(2) or 0)
    return hours * 60 + minutes


def add_minutes_to_time(time_str: str, minutes: int) -> str:
    base = datetime.strptime(time_str, "%H:%M:%S")
    total_seconds = (
        base.hour * 3600 + base.minute * 60 + base.second + minutes * 60
    ) % 86400
    h, rem = divmod(total_seconds, 3600)
    m, s = divmod(rem, 60)
    return f"{h:02d}:{m:02d}:{s:02d}"


def time_in_window(current: str, start: str, end: str) -> bool:
    def to_sec(t):
        h, m, s = map(int, t.split(":"))
        return h * 3600 + m * 60 + s

    c, s, e = to_sec(current), to_sec(start), to_sec(end)
    if s <= e:
        return s <= c < e
    return c >= s or c < e


# ── Endpoints ─────────────────────────────────────────────────────────────────

@app.put("/settings")
def put_settings(body: SettingsIn):
    total_minutes = parse_duration_to_minutes(body.light_duration)
    light_time_off = add_minutes_to_time(body.user_light, total_minutes)

    doc = {
        "user_temp": body.user_temp,
        "user_light": body.user_light,
        "light_duration": body.light_duration,
        "light_time_off": light_time_off,
    }

    settings_collection.replace_one({}, doc, upsert=True)

    return {
        "user_temp": body.user_temp,
        "user_light": body.user_light,
        "light_time_off": light_time_off,
    }


@app.post("/data")
def post_data(body: SensorData):
    now = datetime.now(JAMAICA_TZ).replace(microsecond=0)
    doc = {
        "temperature": body.temperature,
        "presence": body.presence,
        "datetime": now.isoformat(),
    }
    data_collection.insert_one(doc)
    return {
        "temperature": body.temperature,
        "presence": body.presence,
        "datetime": now.isoformat(),
    }


@app.get("/state")
def get_state():
    settings = settings_collection.find_one({}, {"_id": 0})
    latest = data_collection.find_one({}, sort=[("datetime", -1)])

    fan = False
    light = False

    if settings and latest:
        temp = latest.get("temperature", 0)
        presence = latest.get("presence", False)
        user_temp = settings.get("user_temp", 0)
        user_light = settings.get("user_light", "00:00:00")
        light_time_off = settings.get("light_time_off", "00:00:00")

        current_time = datetime.now(JAMAICA_TZ).strftime("%H:%M:%S")

        fan = temp > user_temp and presence
        light = time_in_window(current_time, user_light, light_time_off) and presence

    return {"fan": fan, "light": light}


@app.get("/graph")
def get_graph(size: int = Query(default=10, ge=1)):
    cursor = data_collection.find(
        {}, {"_id": 0}
    ).sort("datetime", -1).limit(size)

    readings = list(cursor)
    readings.reverse()
    return readings


@app.get("/time")
def get_time():
    return {"server_time": datetime.now(JAMAICA_TZ).strftime("%H:%M:%S")}