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
    
    let keywords = command.split(' ');
    const action = keywords[0].toLowerCase();
    keywords = keywords.slice(1); // Remove action word

    if (action === "ekle") {
        let targetDate = new Date();
        const lowerCmd = keywords.map(w => w.toLowerCase());
        
        // Basic NLP for 'yarın' (tomorrow)
        if (lowerCmd.includes("yarın") || lowerCmd.includes("yarin")) {
            targetDate.setDate(targetDate.getDate() + 1);
            keywords = keywords.filter(w => w.toLowerCase() !== "yarın" && w.toLowerCase() !== "yarin");
        }

        // Basic NLP for 'saat' (time)
        const saatIndex = keywords.findIndex(w => w.toLowerCase() === "saat");
        if (saatIndex !== -1 && saatIndex + 1 < keywords.length) {
            const timeStr = keywords[saatIndex + 1];
            const [hour, minute] = timeStr.split(':');
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
    } else if (action === "sil") {
        return { action: "delete", target: keywords.join(' ') };
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
        
        const events = response.data.items.map(event => {
            let timeString = "";
            if (event.start.dateTime) {
                const date = new Date(event.start.dateTime);
                let hours = date.getHours();
                let minutes = date.getMinutes();
                const ampm = hours >= 12 ? 'pm' : 'am';
                hours = hours % 12;
                hours = hours ? hours : 12; 
                const minutesStr = minutes < 10 ? '0' + minutes : minutes;
                timeString = minutes === 0 ? `@${hours}${ampm}` : `@${hours}:${minutesStr}${ampm}`;
            } else if (event.start.date) {
                timeString = "All Day";
            }
            
            return {
                summary: event.summary,
                timeString: timeString
            };
        });

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
                info.message = `Deleted event matching '${info.target}'`;
            } else {
                info.message = `No upcoming event found matching '${info.target}'`;
            }
        }

        res.json({ status: "success", action_taken: info });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

app.listen(PORT, () => {
    console.log(`Server running on http://localhost:${PORT}`);
});
