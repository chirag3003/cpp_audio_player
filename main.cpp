#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <sqlite3.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <condition_variable>

using namespace std;

// Flag to indicate if the music should stop
atomic<bool> stopMusic(false);
condition_variable cv;
mutex mtx;

// Function to display the current timestamp
void displayTimestamp(Mix_Music* music) {
    int duration = Mix_MusicDuration(music);
    int position;
    while (!stopMusic) {
        position = Mix_GetMusicPosition(music);
        if (position == -1) {
            break;  // Music has stopped playing
        }
        // cout << "Playtime : " << position << " / " << duration << " seconds" << endl;
        cout << "Press 'p' to pause/resume, 's' to stop" << endl;
        // Display the progress bar
        int progress = (position * 100) / duration;
        cout << "[";
        for (int i = 0; i < 50; i++) {
            if (i < progress / 2) {
                cout << "=";
            } else if (i == progress / 2) {
                cout << ">";
            } else {
                cout << " ";
            }
        }
        cout << "] " << progress << "%" << endl;
        
        // Create a delay of 1 second
        this_thread::sleep_for(chrono::seconds(1));

        // Clear the console
        system("clear");
    }
    cv.notify_one(); // Notify the main thread that the music has stopped
}

// Function to handle pause, resume, and stop
void handleInput() {
    char input;
    while (true) {
        cin >> input;
        switch (input) {
            case 'p':
                if (Mix_PausedMusic()) {
                    Mix_ResumeMusic();
                } else {
                    Mix_PauseMusic();
                }
                break;
            case 's':
                stopMusic = true;
                Mix_HaltMusic();
                return;

            case 'r':
                Mix_RewindMusic();
                break;

            case 'q':
                stopMusic = true;
                Mix_HaltMusic();
                return;
                            
            default:
                break;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <music_filename>" << endl;
        return 1;
    }

    // Get the music filename
    auto music_filename = argv[1];

    // Initialize SDL
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        cerr << "SDL_Init failed: " << SDL_GetError() << endl;
        return 1;
    }

    // Initialize SDL_mixer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        cerr << "Mix_OpenAudio failed: " << Mix_GetError() << endl;
        SDL_Quit();
        return 1;
    }

    // Load the MP3 file
    Mix_Music* music = Mix_LoadMUS(music_filename);
    if (!music) {
        cerr << "Mix_LoadMUS failed: " << Mix_GetError() << endl;
        Mix_CloseAudio();
        SDL_Quit();
        return 1;
    }

    // Play the music
    if (Mix_PlayMusic(music, 1) == -1) {
        cerr << "Mix_PlayMusic failed: " << Mix_GetError() << endl;
        Mix_FreeMusic(music);
        Mix_CloseAudio();
        SDL_Quit();
        return 1;
    }

    // Start a thread to display the timestamp
    thread timestampThread(displayTimestamp, music);

    // Start a thread to handle user input
    thread inputThread(handleInput);

    // Wait for the music to finish playing or for stop signal
    {
        unique_lock<mutex> lk(mtx);
        cv.wait(lk, []{ return stopMusic.load(); });
    }

    // Clean up
    Mix_FreeMusic(music);
    Mix_CloseAudio();
    SDL_Quit();

    // Ensure threads are joined
    timestampThread.join();
    inputThread.join();

    return 0;
}