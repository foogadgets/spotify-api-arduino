/*******************************************************************
    Prints your currently playing track on spotify to the
    serial monitor using an ES32

    You will need a bearer token, as a temp solution, go to this URL

    https://developer.spotify.com/console/get-users-currently-playing-track/

    Login, and click the "Get Token" button, this is your bearer token

    Parts:
    D1 Mini ESP8266 * - http://s.click.aliexpress.com/e/uzFUnIe

 *  * = Affilate

    If you find what I do usefuland would like to support me,
    please consider becoming a sponsor on Github
    https://github.com/sponsors/witnessmenow/


    Written by Brian Lough
    YouTube: https://www.youtube.com/brianlough
    Tindie: https://www.tindie.com/stores/brianlough/
    Twitter: https://twitter.com/witnessmenow
 *******************************************************************/


// ----------------------------
// Standard Libraries
// ----------------------------

#include <WiFi.h>
#include <WiFiClientSecure.h>

// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------

#include <ArduinoSpotify.h>
// Library for connecting to the Spotify API

// Install from Github
// https://github.com/witnessmenow/arduino-spotify-api

#include <ArduinoJson.h>
// Library used for parsing Json from the API responses

// Search for "Arduino Json" in the Arduino Library manager
// https://github.com/bblanchon/ArduinoJson

//------- Replace the following! ------

char ssid[] = "SSID";         // your network SSID (name)
char password[] = "password"; // your network password

char clientId[] = "56t4373258u3405u43u543"; // Your client ID of your spotify APP
char clientSecret[] = "56t4373258u3405u43u543"; // Your client Secret of your spotify APP (Do Not share this!)

// Country code, including this is advisable
#define SPOTIFY_MARKET "IE"

#define SPOTIFY_REFRESH_TOKEN "AAAAAAAAAABBBBBBBBBBBCCCCCCCCCCCDDDDDDDDDDD"


//------- ---------------------- ------

// including a "spotify_server_cert" variable
// header is included as part of the ArduinoSpotify libary
#include <ArduinoSpotifyCert.h>

WiFiClientSecure client;
ArduinoSpotify spotify(client, clientId, clientSecret, SPOTIFY_REFRESH_TOKEN);

unsigned long delayBetweenRequests = 60000; // Time between requests (1 minute)
unsigned long requestDueTime;               //time when request due


void setup() {

  Serial.begin(115200);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println("");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    client.setCACert(spotify_server_cert);

    // If you want to enable some extra debugging
    // spotify._debug = true;
    Serial.println("Refreshing Access Tokens");
    if(!spotify.refreshAccessToken()){
        Serial.println("Failed to get access tokens");
    }
}

void printCurrentlyPlayingToSerial(CurrentlyPlaying currentlyPlaying)
{
    if (!currentlyPlaying.error)
    {
        Serial.println("--------- Currently Playing ---------");

    
        Serial.print("Is Playing: ");
        if (currentlyPlaying.isPlaying)
        {
            Serial.println("Yes");
        } else {
            Serial.println("No");
        }

        Serial.print("Track: ");
        Serial.println(currentlyPlaying.trackName);

        Serial.print("Artist: ");
        Serial.println(currentlyPlaying.firstArtistName);

        Serial.print("Album: ");
        Serial.println(currentlyPlaying.albumName);

        Serial.println("------------------------");
    }
}

void loop() {
  if (millis() > requestDueTime)
    {
        Serial.print("Free Heap: ");
        Serial.println(ESP.getFreeHeap());

        Serial.println("getting currently playing song:");
        // Market can be excluded if you want e.g. spotify.getCurrentlyPlaying()
        CurrentlyPlaying currentlyPlaying = spotify.getCurrentlyPlaying(SPOTIFY_MARKET);

        printCurrentlyPlayingToSerial(currentlyPlaying);

        requestDueTime = millis() + delayBetweenRequests;
    }

}
