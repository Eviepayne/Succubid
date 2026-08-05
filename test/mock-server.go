package main

import (
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"strings"
	"sync"
	"time"
)

type ServerState struct {
	mu           sync.Mutex
	mode         int
	hsspSetupURL string
	isPlaying    bool
	playTime     int64
}

var state = ServerState{
	mode: 0,
}

type statusRecorder struct {
	http.ResponseWriter
	statusCode int
	length     int
}

func (rec *statusRecorder) WriteHeader(code int) {
	rec.statusCode = code
	rec.ResponseWriter.WriteHeader(code)
}

func (rec *statusRecorder) Write(b []byte) (int, error) {
	if rec.statusCode == 0 {
		rec.statusCode = http.StatusOK
	}
	n, err := rec.ResponseWriter.Write(b)
	rec.length += n
	return n, err
}

func main() {
	mux := http.NewServeMux()

	fw3 := http.NewServeMux()

	fw3.HandleFunc("/status", handleStatusFW3)
	fw3.HandleFunc("/mode", func(w http.ResponseWriter, r *http.Request) {
		switch r.Method {
		case http.MethodGet:
			handleStatusFW3(w, r)
		default:
			handleSetModeFW3(w, r)
		}
	})
	fw3.HandleFunc("/servertime", handleServerTimeFW3)
	fw3.HandleFunc("/upload", handleUpload)
	fw3.HandleFunc("/hssp/setup", handleHSSPSetupFW3)
	fw3.HandleFunc("/hssp/play", func(w http.ResponseWriter, r *http.Request) {
		handleHSSPPlay(w, r, false)
	})
	fw3.HandleFunc("/hssp/stop", handleHSSPStop)

	fw4 := http.NewServeMux()

	fw4.HandleFunc("/mode", handleStatusFW4)
	fw4.HandleFunc("/mode2", handleSetModeFW4)
	fw4.HandleFunc("/servertime", handleServerTimeFW4)
	fw4.HandleFunc("/upload", handleUpload)
	fw4.HandleFunc("/hssp/setup", handleHSSPSetupFW4)
	fw4.HandleFunc("/hssp/play", func(w http.ResponseWriter, r *http.Request) {
		handleHSSPPlay(w, r, true)
	})
	fw4.HandleFunc("/hssp/stop", handleHSSPStop)

	mux.Handle(
		"/fw3/",
		http.StripPrefix("/fw3", authMiddleware(fw3)),
	)

	mux.Handle(
		"/fw4/",
		http.StripPrefix("/fw4", authMiddleware(fw4)),
	)

	port := ":8080"

	fmt.Printf(
		"Mock Handy API Server listening on http://localhost%s\n",
		port,
	)

	log.Fatal(
		http.ListenAndServe(port, mux),
	)
}

func authMiddleware(next http.Handler) http.Handler {

	return http.HandlerFunc(func(
		w http.ResponseWriter,
		r *http.Request,
	) {
		start := time.Now()

		connKey := r.Header.Get("X-Connection-Key")

		if connKey == "" {
			http.Error(
				w,
				`{"error":"Missing X-Connection-Key header"}`,
				http.StatusUnauthorized,
			)
			logTelemetry(r, http.StatusUnauthorized, time.Since(start), "Auth failure: Missing X-Connection-Key")
			return
		}

		if strings.HasPrefix(r.URL.Path, "/fw4") {

			if r.Header.Get("X-Api-Key") == "" {

				http.Error(
					w,
					`{"error":"Missing X-Api-Key header"}`,
					http.StatusUnauthorized,
				)
				logTelemetry(r, http.StatusUnauthorized, time.Since(start), "Auth failure: Missing X-Api-Key")
				return
			}
		}

		w.Header().Set(
			"Content-Type",
			"application/json",
		)

		recorder := &statusRecorder{ResponseWriter: w, statusCode: http.StatusOK}
		next.ServeHTTP(recorder, r)

		duration := time.Since(start)
		logTelemetry(r, recorder.statusCode, duration, fmt.Sprintf("%d bytes returned", recorder.length))
	})
}

func logTelemetry(r *http.Request, statusCode int, duration time.Duration, extra string) {
	log.Printf(
		"[TELEMETRY] Method=%s Path=%s Status=%d Duration=%v RemoteAddr=%s | %s",
		r.Method,
		r.URL.Path,
		statusCode,
		duration,
		r.RemoteAddr,
		extra,
	)
}

func logStateChange(event string, details string) {
	log.Printf("[STATE TELEMETRY] Event=%s | Details: %s", event, details)
}

func handleStatusFW3(
	w http.ResponseWriter,
	r *http.Request,
) {

	state.mu.Lock()
	mode := state.mode
	state.mu.Unlock()

	json.NewEncoder(w).Encode(
		map[string]interface{}{
			"mode":   mode,
			"status": 1,
		},
	)
}

func handleSetModeFW3(
	w http.ResponseWriter,
	r *http.Request,
) {

	setMode(w, r, false)
}

func handleServerTimeFW3(
	w http.ResponseWriter,
	r *http.Request,
) {

	json.NewEncoder(w).Encode(
		map[string]interface{}{
			"serverTime": time.Now().UnixMilli(),
		},
	)
}

func handleStatusFW4(
	w http.ResponseWriter,
	r *http.Request,
) {

	state.mu.Lock()
	mode := state.mode
	state.mu.Unlock()

	json.NewEncoder(w).Encode(
		map[string]interface{}{
			"result": map[string]interface{}{
				"mode": mode,
			},
		},
	)
}

func handleSetModeFW4(
	w http.ResponseWriter,
	r *http.Request,
) {

	setMode(w, r, true)
}

func handleServerTimeFW4(
	w http.ResponseWriter,
	r *http.Request,
) {

	json.NewEncoder(w).Encode(
		map[string]interface{}{
			"server_time": time.Now().UnixMilli(),
		},
	)
}

func setMode(
	w http.ResponseWriter,
	r *http.Request,
	fw4 bool,
) {

	var body struct {
		Mode int `json:"mode"`
	}

	err := json.NewDecoder(r.Body).Decode(&body)

	if err != nil {

		http.Error(
			w,
			`{"error":"Invalid payload"}`,
			http.StatusBadRequest,
		)

		return
	}

	state.mu.Lock()
	oldMode := state.mode
	state.mode = body.Mode
	state.mu.Unlock()

	logStateChange("MODE_CHANGE", fmt.Sprintf("Mode transitioned from %d -> %d", oldMode, body.Mode))

	if fw4 {

		json.NewEncoder(w).Encode(
			map[string]interface{}{
				"result": 0,
			},
		)

	} else {

		json.NewEncoder(w).Encode(
			map[string]interface{}{
				"result": 0,
			},
		)
	}
}

func handleUpload(
	w http.ResponseWriter,
	r *http.Request,
) {

	err := r.ParseMultipartForm(
		20 << 20,
	)

	if err != nil {

		http.Error(
			w,
			`{"error":"multipart failed"}`,
			400,
		)

		return
	}

	file, header, err :=
		r.FormFile("file")

	if err != nil {

		http.Error(
			w,
			`{"error":"missing file"}`,
			400,
		)

		return
	}

	defer file.Close()

	logStateChange("FILE_UPLOAD", fmt.Sprintf("Uploaded script '%s' (%d bytes)", header.Filename, header.Size))

	json.NewEncoder(w).Encode(
		map[string]interface{}{
			"success": true,
			"url": fmt.Sprintf(
				"http://localhost:8080/scripts/%s",
				header.Filename,
			),
		},
	)
}

func handleHSSPSetupFW3(
	w http.ResponseWriter,
	r *http.Request,
) {

	handleHSSPSetup(w, r, false)

}

func handleHSSPSetupFW4(
	w http.ResponseWriter,
	r *http.Request,
) {

	handleHSSPSetup(w, r, true)

}

func handleHSSPSetup(
	w http.ResponseWriter,
	r *http.Request,
	fw4 bool,
) {

	var body struct {
		URL string `json:"url"`
	}

	json.NewDecoder(r.Body).Decode(&body)

	state.mu.Lock()
	state.hsspSetupURL = body.URL
	state.mu.Unlock()

	logStateChange("HSSP_SETUP", fmt.Sprintf("Configured URL: %s (FW4: %v)", body.URL, fw4))

	json.NewEncoder(w).Encode(
		map[string]interface{}{
			"result": 0,
		},
	)
}

func handleHSSPPlay(
	w http.ResponseWriter,
	r *http.Request,
	fw4 bool,
) {

	state.mu.Lock()
	defer state.mu.Unlock()

	if state.hsspSetupURL == "" {

		http.Error(
			w,
			`{"error":"No script configured"}`,
			400,
		)

		return
	}

	var body map[string]interface{}

	data, _ := io.ReadAll(r.Body)

	json.Unmarshal(data, &body)

	if fw4 {

		if v, ok := body["start_time"].(float64); ok {
			state.playTime = int64(v)
		}

	} else {

		if v, ok := body["startTime"].(float64); ok {
			state.playTime = int64(v)
		}
	}

	state.isPlaying = true

	logStateChange("HSSP_PLAY", fmt.Sprintf("Started playing script from startTime: %d ms (FW4: %v)", state.playTime, fw4))

	json.NewEncoder(w).Encode(
		map[string]interface{}{
			"result": 0,
		},
	)
}

func handleHSSPStop(
	w http.ResponseWriter,
	r *http.Request,
) {

	state.mu.Lock()
	state.isPlaying = false
	state.mu.Unlock()

	logStateChange("HSSP_STOP", "Stopped playback")

	json.NewEncoder(w).Encode(
		map[string]interface{}{
			"result": 0,
		},
	)
}
