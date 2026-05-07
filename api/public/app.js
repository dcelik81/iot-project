const voiceBtn = document.getElementById('voice-btn');
const instruction = document.getElementById('instruction');
const statusIndicator = document.getElementById('status-indicator');
const eventsList = document.getElementById('events-list');

let mediaRecorder;
let audioChunks = [];
let isRecording = false;

// Fetch Events on Load
async function fetchEvents() {
    try {
        const response = await fetch('/events');
        const data = await response.json();
        renderEvents(data.events);
    } catch (error) {
        console.error('Error fetching events:', error);
        eventsList.innerHTML = '<p class="error">Failed to load events</p>';
    }
}

function renderEvents(events) {
    if (!events || events.length === 0) {
        eventsList.innerHTML = '<p class="empty">No upcoming events</p>';
        return;
    }

    eventsList.innerHTML = events.map(event => `
        <div class="event-item">
            <span class="summary">${event.summary}</span>
            <span class="time">${event.timeString}</span>
        </div>
    `).join('');
}

// Recording Logic
async function startRecording() {
    try {
        const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
        mediaRecorder = new MediaRecorder(stream);
        audioChunks = [];

        mediaRecorder.ondataavailable = (event) => {
            audioChunks.push(event.data);
        };

        mediaRecorder.onstop = async () => {
            const audioBlob = new Blob(audioChunks, { type: 'audio/wav' });
            await sendAudio(audioBlob);
        };

        mediaRecorder.start();
        isRecording = true;
        updateUI();
    } catch (err) {
        console.error('Microphone access denied:', err);
        alert('Please allow microphone access to use voice commands.');
    }
}

function stopRecording() {
    if (mediaRecorder && isRecording) {
        mediaRecorder.stop();
        isRecording = false;
        updateUI();
    }
}

async function sendAudio(blob) {
    instruction.innerText = "Transcribing...";
    const formData = new FormData();
    formData.append('audio', blob, 'recording.wav');

    try {
        const response = await fetch('/transcribe', {
            method: 'POST',
            body: formData
        });
        const result = await response.json();
        
        if (result.status === 'success') {
            instruction.innerText = `Command: ${result.transcription}`;
            fetchEvents(); // Refresh list
        } else {
            instruction.innerText = "Error: Could not understand";
        }
    } catch (error) {
        console.error('Error uploading audio:', error);
        instruction.innerText = "Upload failed";
    }

    setTimeout(() => {
        if (!isRecording) instruction.innerText = "Tap to Speak";
    }, 3000);
}

function updateUI() {
    if (isRecording) {
        voiceBtn.classList.add('is-recording');
        statusIndicator.classList.remove('online');
        statusIndicator.classList.add('recording');
        instruction.innerText = "Listening...";
    } else {
        voiceBtn.classList.remove('is-recording');
        statusIndicator.classList.remove('recording');
        statusIndicator.classList.add('online');
    }
}

// Event Listeners
voiceBtn.addEventListener('mousedown', startRecording);
voiceBtn.addEventListener('mouseup', stopRecording);

// Touch events for mobile
voiceBtn.addEventListener('touchstart', (e) => {
    e.preventDefault();
    startRecording();
});
voiceBtn.addEventListener('touchend', (e) => {
    e.preventDefault();
    stopRecording();
});

// Initial Load
fetchEvents();
