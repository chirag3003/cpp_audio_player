#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <sqlite3.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <vector>
#include <string>

using namespace std;

// Flag to indicate if the music should stop
atomic<bool> stopMusic(false);
condition_variable cv;
mutex mtx;

class MusicInput
{
public:
    vector<string> musicFiles;

    void fetchMusicList(string folderPath)
    {
        printf("folderPath = %s\n", folderPath.c_str());
        vector<string> _musicFiles;
        for (const auto &entry : filesystem::directory_iterator(folderPath))
        {
            if (entry.is_regular_file())
            {
                string filePath = entry.path().string();
                // Check if the file is a music file (e.g., .mp3, .wav)
                if (filePath.find(".mp3") != string::npos || filePath.find(".wav") != string::npos)
                {
                    _musicFiles.push_back(filePath);
                }
            }
        }
        musicFiles = _musicFiles;
    }

    void printList()
    {
        cout << "Music Options: " << endl;
        for (int i = 0; i < musicFiles.size(); i++)
        {
            cout << i << ". " << musicFiles[i] << endl;
        }
    };
};

class Music
{
public:
    string filePath;
    static void displayTimestamp(Mix_Music *music)
    {
        int duration = Mix_MusicDuration(music);
        int position;
        while (!stopMusic)
        {
            position = Mix_GetMusicPosition(music);
            if (position == -1)
            {
                break; // Music has stopped playing
            }
            cout << "Press 'p' to pause/resume, 's' to stop" << endl;
            // Display the progress bar
            int progress = (position * 100) / duration;
            cout << "[";
            for (int i = 0; i < 50; i++)
            {
                if (i < progress / 2)
                {
                    cout << "=";
                }
                else if (i == progress / 2)
                {
                    cout << ">";
                }
                else
                {
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
    static void handleInput()
    {
        char input;
        while (true)
        {
            cin >> input;
            switch (input)
            {
            case 'p':
                if (Mix_PausedMusic())
                {
                    Mix_ResumeMusic();
                }
                else
                {
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

            default:
                break;
            }
        }
    }

public:
    Music(string _filePath) : filePath(_filePath) {}

    string getFilePath() const { return filePath; }

    void displayInfo() const
    {
        cout << "File Path: " << filePath << endl;
    }

    void play()
    {
        // Initialize SDL
        if (SDL_Init(SDL_INIT_AUDIO) < 0)
        {
            cerr << "SDL_Init failed: " << SDL_GetError() << endl;
            return;
        }

        // Initialize SDL_mixer
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
        {
            cerr << "Mix_OpenAudio failed: " << Mix_GetError() << endl;
            SDL_Quit();
            return;
        }

        // Load the MP3 file
        Mix_Music *music = Mix_LoadMUS(filePath.c_str());
        if (!music)
        {
            cerr << "Mix_LoadMUS failed: " << Mix_GetError() << endl;
            Mix_CloseAudio();
            SDL_Quit();
            return;
        }

        // Play the music
        if (Mix_PlayMusic(music, 1) == -1)
        {
            cerr << "Mix_PlayMusic failed: " << Mix_GetError() << endl;
            Mix_FreeMusic(music);
            Mix_CloseAudio();
            SDL_Quit();
            return;
        }

        // Reset stopMusic flag
        stopMusic = false;

        // Start a thread to display the timestamp
        thread timestampThread(displayTimestamp, music);

        // Start a thread to handle user input
        thread inputThread(handleInput);

        // Wait for the music to finish playing or for stop signal
        {
            unique_lock<mutex> lk(mtx);
            cv.wait(lk, []
                    { return stopMusic.load(); });
        }

        // Clean up
        Mix_FreeMusic(music);
        Mix_CloseAudio();
        SDL_Quit();

        // Ensure threads are joined
        timestampThread.join();
        inputThread.join();
    }
};

class Playlist
{
public:
    vector<Music *> musicObjects;

    Playlist(const vector<string> &musicFiles)
    {
        for (const auto &file : musicFiles)
        {
            musicObjects.push_back(new Music(file));
        }
    }

    ~Playlist()
    {
        for (auto music : musicObjects)
        {
            delete music;
        }
    }

    void printList()
    {
        cout << "Playlist: " << endl;
        for (int i = 0; i < musicObjects.size(); i++)
        {
            cout << i << ". " << musicObjects[i]->getFilePath() << endl;
        }
    }

    Music *getMusic(int index)
    {
        if (index < 0 || index >= musicObjects.size())
        {
            return nullptr;
        }
        return musicObjects[index];
    }
};

class Player
{
    Playlist playlist;

public:
    Player(Playlist &_playlist) : playlist(_playlist) {}

    int start()
    {
        while (true)
        {
            int choice = 0;
            playlist.printList();
            cout << "Enter the number of the music file you want to play (or -1 to exit): ";
            cin >> choice;

            if (choice == -1)
            {
                break;
            }

            Music *music = playlist.getMusic(choice);
            if (!music)
            {
                cerr << "Invalid choice. Please try again." << endl;
                continue;
            }

            music->play();
        }

        return 0;
    }
};

int main()
{
    string folderPath;
    cout << "Enter the folder path containing music files: ";
    cin >> folderPath;

    auto musicInput = MusicInput();
    musicInput.fetchMusicList(folderPath);
    Playlist playlist(musicInput.musicFiles);
    Player player(playlist);
    return player.start();
}