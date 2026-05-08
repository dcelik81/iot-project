const { google } = require('googleapis');
const express = require('express');
const dotenv = require('dotenv');
const cors = require('cors');
const multer = require('multer');
const fs = require('fs');
const path = require('path');
const speech = require('@google-cloud/speech');

const upload = multer({ dest: 'uploads/' });
const speechClient = new speech.SpeechClient({
    keyFilename: process.env.GOOGLE_APPLICATION_CREDENTIALS || 'service-account.json'
});

dotenv.config();

const app = express();
app.use(express.json());
app.use(cors());
app.use(express.static(path.join(__dirname, 'public')));

let esp32State = "MENU"; // Global state tracker

const PORT = process.env.PORT || 8000;

// Simple Keyword NLP Logic
const extractEventInfo = (command) => {
    if (!command) return null;

    let keywords = command.split(' ');
    const action = keywords[0].toLowerCase();
    keywords = keywords.slice(1); // Remove action word

    if (action === "ekle") {
        let targetDate = new Date();
        const lowerCmd = keywords.map(w => w.toLowerCase());

        // Basic NLP for 'yarın' (tomorrow)
        if (lowerCmd.includes("yarın") || lowerCmd.includes("yarin")) {
            targetDate.setDate(targetDate.getDate() + 1);
            const yarinIndex = lowerCmd.findIndex(w => w === "yarın" || w === "yarin");
            keywords.splice(yarinIndex, 1);
        }

        // Time parsing (e.g., "saat 14", "saat 14:30")
        const saatIndex = keywords.findIndex(w => w.toLowerCase() === "saat");
        if (saatIndex !== -1 && keywords[saatIndex + 1]) {
            const timeStr = keywords[saatIndex + 1];
            const [hour, minute] = timeStr.split(/[:.]/);
            targetDate.setHours(parseInt(hour, 10), minute ? parseInt(minute, 10) : 0, 0, 0);
            keywords.splice(saatIndex, 2); // Remove "saat" and the time value
        } else {
            // Default: if no specific time is given, set it to the next full hour
            targetDate.setHours(targetDate.getHours() + 1, 0, 0, 0);
        }

        const summary = keywords.join(' ') || "Yeni Etkinlik";

        return {
            action: "add",
            summary: summary,
            startTime: targetDate.toISOString(),
            endTime: new Date(targetDate.getTime() + 3600000).toISOString() // 1 hour duration
        };
    } else if (action === "sil" || action === "iptal" || action === "kaldır") {
        return { action: "delete", target: keywords.join(' ') };
    } else if (action === "etkinlik" || action === "listele" || action === "liste") {
        return { action: "list" };
    } else if (action === "bekle" || action === "idle") {
        return { action: "idle" };
    } else if (action === "menü" || action === "menu") {
        return { action: "menu" };
    }

    return null;
};

// Google Calendar API Setup
const auth = new google.auth.GoogleAuth({
    keyFile: process.env.GOOGLE_APPLICATION_CREDENTIALS,
    scopes: ['https://www.googleapis.com/auth/calendar'],
});

const calendar = google.calendar({ version: 'v3', auth });

app.get('/', (req, res) => {
    res.json({ status: "IoT Assistant API (Node.js) is running" });
});

async function handleCalendarAction(info) {
    if (info.action === 'add') {
        const event = {
            summary: info.summary,
            start: { dateTime: info.startTime },
            end: { dateTime: info.endTime },
        };
        const response = await calendar.events.insert({
            calendarId: process.env.CALENDAR_ID || 'primary',
            resource: event
        });
        info.eventId = response.data.id;
    } else if (info.action === 'delete') {
        const calendarId = process.env.CALENDAR_ID || 'primary';
        const searchResponse = await calendar.events.list({
            calendarId: calendarId,
            q: info.target,
            timeMin: (new Date()).toISOString(),
            maxResults: 1,
            singleEvents: true,
            orderBy: 'startTime',
        });

        const events = searchResponse.data.items;
        if (events && events.length > 0) {
            const eventId = events[0].id;
            await calendar.events.delete({
                calendarId: calendarId,
                eventId: eventId
            });
            info.deletedEventId = eventId;
            info.message = `Silinen Etkinlik: '${info.target}'`;
        } else {
            info.message = `Arama Kelimesi ile Etkinlik Bulunamadı: '${info.target}'`;
        }
    } else if (info.action === 'idle') {
        esp32State = "IDLE";
        info.message = "Bekleme Moduna Geçiliyor";
    } else if (info.action === 'menu') {
        esp32State = "MENU";
        info.message = "Menüye Geçiliyor";
    }
    return info;
}

app.get('/state', (req, res) => {
    res.json({ state: esp32State });
});

const replaceTurkish = (text) => {
    if (!text) return "";
    const map = {
        'ç': 'c', 'Ç': 'C', 'ğ': 'g', 'Ğ': 'G', 'ı': 'i', 'İ': 'I',
        'ö': 'o', 'Ö': 'O', 'ş': 's', 'Ş': 'S', 'ü': 'u', 'Ü': 'U'
    };
    return text.replace(/[çÇğĞıİöÖşŞüÜ]/g, match => map[match]);
};

app.get('/events', async (req, res) => {
    try {
        const now = new Date();
        const response = await calendar.events.list({
            calendarId: process.env.CALENDAR_ID || 'primary',
            timeMin: now.toISOString(),
            maxResults: 10,
            singleEvents: true,
            orderBy: 'startTime',
        });

        // Filter out events that started in the past (even if they are ongoing)
        const events = response.data.items.filter(event => {
            const startTime = event.start.dateTime || event.start.date;
            return new Date(startTime) >= now;
        }).map(event => {
            let timeString = "";
            if (event.start.dateTime) {
                const date = new Date(event.start.dateTime);
                const hours = date.getHours().toString().padStart(2, '0');
                const minutes = date.getMinutes().toString().padStart(2, '0');
                timeString = `${hours}:${minutes}`;
            } else if (event.start.date) {
                timeString = "All Day";
            }

            return {
                summary: replaceTurkish(event.summary),
                timeString: timeString
            };
        });

        const currentTime = now.getHours().toString().padStart(2, '0') + ":" + now.getMinutes().toString().padStart(2, '0');

        res.json({
            events: events,
            count: events.length,
            currentTime: currentTime,
            forcedState: esp32State // Let ESP32 know if it should be in a certain state
        });
    } catch (error) {
        console.error('Error fetching events:', error);
        res.status(500).json({ error: "Calendar API error" });
    }
});

app.post('/command', async (req, res) => {
    const { voiceCommand } = req.body;
    const info = extractEventInfo(voiceCommand);

    if (!info) {
        return res.status(400).json({ detail: "Anlaşılamayan komut" });
    }

    try {
        const result = await handleCalendarAction(info);
        res.json({ status: "success", action_taken: result });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

app.post('/transcribe', upload.single('audio'), async (req, res) => {
    if (!req.file) {
        return res.status(400).json({ error: "No audio file uploaded" });
    }

    const audioPath = req.file.path;

    try {
        const fileBuffer = fs.readFileSync(audioPath);
        const audioBytes = fileBuffer.toString('base64');

        const audio = { content: audioBytes };
        const config = {
            encoding: 'WEBM_OPUS', // Default for MediaRecorder on most browsers
            sampleRateHertz: 48000,
            languageCode: 'tr-TR', // Set to Turkish
        };

        const request = { audio: audio, config: config };

        console.log("Transcribing audio...");
        const [response] = await speechClient.recognize(request);
        const transcription = response.results
            .map(result => result.alternatives[0].transcript)
            .join('\n');

        console.log(`Transcription: ${transcription}`);

        const info = extractEventInfo(transcription);
        let actionResult = null;

        if (info) {
            actionResult = await handleCalendarAction(info);
        }

        fs.unlinkSync(audioPath);
        res.json({ status: "success", transcription, action_taken: actionResult });
    } catch (error) {
        console.error('Transcription error:', error);
        if (fs.existsSync(audioPath)) fs.unlinkSync(audioPath);
        res.status(500).json({ error: error.message });
    }
});

app.listen(PORT, () => {
    console.log(`Server running on http://localhost:${PORT}`);
});
