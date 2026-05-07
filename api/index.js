const express = require('express');
const { google } = require('googleapis');
const dotenv = require('dotenv');
const cors = require('cors');

dotenv.config();

const app = express();
app.use(express.json());
app.use(cors());

const PORT = process.env.PORT || 8000;

// Simple Keyword NLP Logic
const extractEventInfo = (command) => {
    if (!command) return null;
    
    const keywords = command.split(' ');
    const action = keywords[0].toLowerCase();

    if (action === "ekle") {
        const summary = keywords.slice(1).join(' ') || "Yeni Etkinlik";
        return { action: "add", summary };
    } else if (action === "sil") {
        return { action: "delete", target: keywords.slice(1).join(' ') };
    } else if (action === "etkinlik") {
        return { action: "list" };
    }
    
    return null;
};

// Google Calendar API Setup (Placeholder)
const auth = new google.auth.GoogleAuth({
    keyFile: process.env.GOOGLE_APPLICATION_CREDENTIALS,
    scopes: ['https://www.googleapis.com/auth/calendar'],
});

const calendar = google.calendar({ version: 'v3', auth });

app.get('/', (req, res) => {
    res.json({ status: "IoT Assistant API (Node.js) is running" });
});

app.get('/events', async (req, res) => {
    try {
        const response = await calendar.events.list({
            calendarId: process.env.CALENDAR_ID || 'primary',
            timeMin: (new Date()).toISOString(),
            maxResults: 10,
            singleEvents: true,
            orderBy: 'startTime',
        });
        const events = response.data.items;
        res.json({ events });
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
        // Implement Google Calendar action based on 'info'
        // Example for 'add':
        if (info.action === 'add') {
            const event = {
                summary: info.summary,
                start: { dateTime: new Date().toISOString() }, // Needs better parsing
                end: { dateTime: new Date(Date.now() + 3600000).toISOString() },
            };
            // await calendar.events.insert({ calendarId: 'primary', resource: event });
        }

        res.json({ status: "success", action_taken: info });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

app.listen(PORT, () => {
    console.log(`Server running on http://localhost:${PORT}`);
});
