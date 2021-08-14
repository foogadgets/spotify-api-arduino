/*
ArduinoSpotify - An Arduino library to wrap the Spotify API

Copyright (c) 2020  Brian Lough.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "ArduinoSpotify.h"

ArduinoSpotify::ArduinoSpotify(Client &client)
{
    this->client = &client;
	memset(this->_clientId, 0, 33*sizeof(char));
	memset(this->_clientSecret, 0, 33*sizeof(char));

    _initCurrentlyPlayingStruct();
}

ArduinoSpotify::ArduinoSpotify(Client &client, char *bearerToken)
{
    this->client = &client;
	memset(this->_bearerToken, 0, SIZEOFACCESS*sizeof(char));
    strncpy(this->_bearerToken, "Bearer ", 7);
	strncat(this->_bearerToken, bearerToken, (SIZEOFACCESS-1-7));

    _initCurrentlyPlayingStruct();
}

ArduinoSpotify::ArduinoSpotify(Client &client, const char *clientId, const char *clientSecret, const char *refreshToken)
{
    this->client = &client;
	memset(this->_clientId, 0, 33*sizeof(char));
    strncpy(this->_clientId, clientId, 32);
	memset(this->_clientSecret, 0, 33*sizeof(char));
    strncpy(this->_clientSecret, clientSecret, 32);
	memset(this->_refreshToken, 0, SIZEOFREFRES*sizeof(char));
    strncpy(this->_refreshToken, refreshToken, (SIZEOFREFRES-1));

    _initCurrentlyPlayingStruct();
}

int ArduinoSpotify::makeRequestWithBody(const char *type, const char *command, const char *authorization, const char *body, const char *contentType, const char *host)
{

    int status = 0;
    client->flush();
    client->setTimeout(SPOTIFY_TIMEOUT);
    status = client->connect(host, portNumber);
    if (!status)
    {
        Serial.print("makeRequestWithBody: ");
        Serial.println(status);
        return status;
    }

    // give the esp a breather
    yield();

    // Send HTTP request
    client->print(type);
    client->print(command);
    client->println(F(" HTTP/1.1"));

    //Headers
    client->print(F("Host: "));
    client->println(host);

    client->println(F("Accept: application/json"));
    client->print(F("Content-Type: "));
    client->println(contentType);

    if (authorization != NULL)
    {
        client->print(F("Authorization: "));
        client->println(authorization);
    }

    client->println(F("Cache-Control: no-cache"));

    client->print(F("Content-Length: "));
    client->println(strlen(body));

    client->println();

    client->print(body);

    if (client->println() == 0)
    {
        Serial.println(F("Failed to send request"));
        return -2;
    }

    int statusCode = getHttpStatusCode();
    return statusCode;
}

int ArduinoSpotify::makePutRequest(const char *command, const char *authorization, const char *body, const char *contentType, const char *host)
{
    return makeRequestWithBody("PUT ", command, authorization, body, contentType);
}

int ArduinoSpotify::makePostRequest(const char *command, const char *authorization, const char *body, const char *contentType, const char *host)
{
    return makeRequestWithBody("POST ", command, authorization, body, contentType, host);
}

int ArduinoSpotify::makeGetRequest(const char *command, const char *authorization, const char *accept, const char *host)
{
    client->flush();
    client->setTimeout(SPOTIFY_TIMEOUT);
    if (!client->connect(host, portNumber))
    {
        Serial.println(F("makeGetRequest: Connection failed"));
        return -1;
    }

    // give the esp a breather
    yield();

    // Send HTTP request
    client->print(F("GET "));
    client->print(command);
    client->println(F(" HTTP/1.1"));

    //Headers
    client->print(F("Host: "));
    client->println(host);

    if (accept != NULL)
    {
        client->print(F("Accept: "));
        client->println(accept);
    }

    if (authorization != NULL)
    {
        client->print(F("Authorization: "));
        client->println(authorization);
    }

    client->println(F("Cache-Control: no-cache"));

    if (client->println() == 0)
    {
        Serial.println(F("Failed to send request"));
        return -2;
    }
    int statusCode = getHttpStatusCode();

    return statusCode;
}

void ArduinoSpotify::setClientId(const char *clientId)
{
	memset(this->_clientId, 0, 33*sizeof(char));
    strncpy(this->_clientId, clientId, 32);
}

void ArduinoSpotify::setClientSecret(const char *clientSecret)
{
	memset(this->_clientSecret, 0, 33*sizeof(char));
    strncpy(this->_clientSecret, clientSecret, 32);
}

void ArduinoSpotify::setRefreshToken(const char *refreshToken)
{
	memset(this->_refreshToken, 0, SIZEOFREFRES*sizeof(char));
    strncpy(this->_refreshToken, refreshToken, (SIZEOFREFRES-1));
}

bool ArduinoSpotify::refreshAccessToken()
{
    char body[310];
	memset(body, 0, 310*sizeof(char));
    sprintf(body, this->refreshAccessTokensBody, this->_refreshToken, this->_clientId, this->_clientSecret);

#ifdef SPOTIFY_DEBUG
	Serial.println("In refreshAccessToken");
    Serial.println(body);
#endif

    int statusCode = makePostRequest(SPOTIFY_TOKEN_ENDPOINT, NULL, body, "application/x-www-form-urlencoded", SPOTIFY_ACCOUNTS_HOST);
    if (statusCode > 0)
    {
        skipHeaders();
    }
    unsigned long now = millis();

#ifdef SPOTIFY_DEBUG
    Serial.print("Status code: ");
    Serial.println(statusCode);
#endif

    bool refreshed = false;
    if (statusCode == 200)
    {
        //DynamicJsonDocument doc(1000);
        DeserializationError error = deserializeJson(doc, *client);
        if (!error)
        {
			memset(this->_bearerToken, 0, SIZEOFACCESS*sizeof(char));
            strncpy(this->_bearerToken, "Bearer ", 7);
            strncat(this->_bearerToken, (char *)doc["access_token"].as<const char *>(), (SIZEOFACCESS-1-7)  );
            int tokenTtl = doc["expires_in"];             // Usually 3600 (1 hour)
			tokenTimeToLiveMs = (tokenTtl * 1000) - 2000; // The 2000 is just to force the token expiry to check if its very close
            timeTokenRefreshed = now;
            refreshed = true;
        }
	doc.clear();
    }
    else
    {
        parseError();
    }

    closeClient();
    return refreshed;
}

bool ArduinoSpotify::checkAndRefreshAccessToken()
{
    unsigned long timeSinceLastRefresh = millis() - timeTokenRefreshed;
    if (timeSinceLastRefresh >= tokenTimeToLiveMs)
    {
        Serial.println("Refresh of the Access token is due, doing that now.");
        return refreshAccessToken();
    }

    // Token is still valid
    return true;
}

const char *ArduinoSpotify::getAccessToken()
{
	return (7*sizeof(char))+this->_bearerToken;
}

const char *ArduinoSpotify::getRefreshToken()
{
	return this->_refreshToken;
}

const char *ArduinoSpotify::requestAccessTokens(const char *code, const char *redirectUrl)
{
    char body[560];
	memset(body, 0, 560*sizeof(char));
    sprintf(body, requestAccessTokensBody, redirectUrl, code, this->_clientId, this->_clientSecret);

#ifdef SPOTIFY_DEBUG
	Serial.println("In requestAccessTokens");
    Serial.println(body);
#endif

    int statusCode = makePostRequest(SPOTIFY_TOKEN_ENDPOINT, NULL, body, "application/x-www-form-urlencoded", SPOTIFY_ACCOUNTS_HOST);
    if (statusCode > 0)
    {
        skipHeaders();
    }
    unsigned long now = millis();

#ifdef SPOTIFY_DEBUG
    Serial.print("status Code");
    Serial.println(statusCode);
#endif

    if (statusCode == 200)
    {
        //DynamicJsonDocument doc(1000);
        DeserializationError error = deserializeJson(doc, *client);
        if (!error)
        {
			memset(this->_bearerToken, 0, SIZEOFACCESS*sizeof(char));
            strncpy(this->_bearerToken, "Bearer ", 7);
			strncat(this->_bearerToken, (char *)doc["access_token"].as<const char *>(), (SIZEOFACCESS-1-7));
	        memset(this->_refreshToken, 0, SIZEOFREFRES*sizeof(char));
            strncpy(this->_refreshToken, (char *)doc["refresh_token"].as<const char *>(), (SIZEOFREFRES-1));
            int tokenTtl = doc["expires_in"];             // Usually 3600 (1 hour)
            tokenTimeToLiveMs = (tokenTtl * 1000) - 2000; // The 2000 is just to force the token expiry to check if its very close
            timeTokenRefreshed = now;
        }
	doc.clear();
    }
    else
    {
        parseError();
    }

    closeClient();
    return this->_refreshToken;
}

bool ArduinoSpotify::play(const char *deviceId)
{
    memset(command, 0, 125*sizeof(char));
    strncpy(command, SPOTIFY_PLAY_ENDPOINT, 124);
    return playerControl(command, deviceId);
}

bool ArduinoSpotify::playAdvanced(char *body, const char *deviceId)
{
    memset(command, 0, 125*sizeof(char));
    strncpy(command, SPOTIFY_PLAY_ENDPOINT, 124);
    return playerControl(command, deviceId, body);
}

bool ArduinoSpotify::pause(const char *deviceId)
{
    memset(command, 0, 125*sizeof(char));
    strncpy(command, SPOTIFY_PAUSE_ENDPOINT, 124);
    return playerControl(command, deviceId);
}

bool ArduinoSpotify::setVolume(int volume, const char *deviceId)
{
    memset(command, 0, 125*sizeof(char));
    sprintf(command, SPOTIFY_VOLUME_ENDPOINT, volume);
    return playerControl(command, deviceId);
}

bool ArduinoSpotify::toggleShuffle(bool shuffle, const char *deviceId)
{
    memset(command, 0, 125*sizeof(char));
    char shuffleState[6];
    memset(shuffleState, 0, 6*sizeof(char));
    if (shuffle)
    {
        strncpy(shuffleState, "true", 5);
    }
    else
    {
        strncpy(shuffleState, "false", 5);
    }
    sprintf(command, SPOTIFY_SHUFFLE_ENDPOINT, shuffleState);
    return playerControl(command, deviceId);
}

bool ArduinoSpotify::setRepeatMode(RepeatOptions repeat, const char *deviceId)
{
    memset(command, 0, 125*sizeof(char));
    char repeatState[8];
	memset(repeatState, 0, 8*sizeof(char));
    switch (repeat)
    {
    case repeat_track:
        strncpy(repeatState, "track", 5);
        break;
    case repeat_context:
        strncpy(repeatState, "context", 7);
        break;
    case repeat_off:
        strncpy(repeatState, "off", 3);
        break;
    }

    sprintf(command, SPOTIFY_REPEAT_ENDPOINT, repeatState);
    return playerControl(command, deviceId);
}

bool ArduinoSpotify::playerControl(char *command, const char *deviceId, const char *body)
{
    if (deviceId[0] != 0)
    {
        char *questionMarkPointer;
        questionMarkPointer = strchr(command, '?');
        char deviceIdBuff[50];
		memset(deviceIdBuff, 0, 50*sizeof(char));
        if (questionMarkPointer == NULL)
        {
            sprintf(deviceIdBuff, "?deviceId=%s", deviceId);
        }
        else
        {
            // params already started
            sprintf(deviceIdBuff, "&deviceId=%s", deviceId);
        }
        strcat(command, deviceIdBuff);
    }

#ifdef SPOTIFY_DEBUG
    Serial.println(command);
    Serial.println(body);
#endif

    if (autoTokenRefresh)
    {
        checkAndRefreshAccessToken();
    }
    int statusCode = makePutRequest(command, this->_bearerToken, body);

    closeClient();
    //Will return 204 if all went well.
    return statusCode == 204;
}

bool ArduinoSpotify::playerNavigate(char *command, const char *deviceId)
{
    if (deviceId[0] != 0)
    {
        char deviceIdBuff[50];
		memset(deviceIdBuff, 0, 50*sizeof(char));
        sprintf(deviceIdBuff, "?deviceId=%s", deviceId);
        strcat(command, deviceIdBuff);
    }

#ifdef SPOTIFY_DEBUG
    Serial.println(command);
#endif

    if (autoTokenRefresh)
    {
        checkAndRefreshAccessToken();
    }
    int statusCode = makePostRequest(command, this->_bearerToken);

    closeClient();
    //Will return 204 if all went well.
    return statusCode == 204;
}

bool ArduinoSpotify::nextTrack(const char *deviceId)
{
    memset(command, 0, 125*sizeof(char));
    strncpy(command, SPOTIFY_NEXT_TRACK_ENDPOINT, 124);
    return playerNavigate(command, deviceId);
}

bool ArduinoSpotify::previousTrack(const char *deviceId)
{
    memset(command, 0, 125*sizeof(char));
    strncpy(command, SPOTIFY_PREVIOUS_TRACK_ENDPOINT, 124);
    return playerNavigate(command, deviceId);
}

bool ArduinoSpotify::seek(int position, const char *deviceId)
{
    memset(command, 0, 125*sizeof(char));
    strncpy(command, SPOTIFY_SEEK_ENDPOINT, 124);
    char tempBuff[50];
    memset(tempBuff, 0, 50*sizeof(char));
    sprintf(tempBuff, "?position_ms=%d", position);
    strncat(command, tempBuff, 50);
    if (deviceId[0] != 0)
    {
        sprintf(tempBuff, "?deviceId=%s", deviceId);
        strcat(command, tempBuff);
    }

#ifdef SPOTIFY_DEBUG
    Serial.println(command);
#endif

    if (autoTokenRefresh)
    {
        checkAndRefreshAccessToken();
    }
    int statusCode = makePutRequest(command, this->_bearerToken);
    closeClient();
    //Will return 204 if all went well.
    return statusCode == 204;
}

CurrentlyPlaying* ArduinoSpotify::getCurrentlyPlaying(const char *market)
{
	_initCurrentlyPlayingStruct();

    memset(command, 0, 125*sizeof(char));
    strncpy(command, SPOTIFY_CURRENTLY_PLAYING_ENDPOINT, 124);
    if (market[0] != 0)
    {
        char marketBuff[30];
		memset(marketBuff, 0, 30*sizeof(char));
        sprintf(marketBuff, "?market=%s", market);
        strncat(command, marketBuff, (100-1-10));
    }

#ifdef SPOTIFY_DEBUG
    Serial.println(command);
#endif

    // Get from https://arduinojson.org/v6/assistant/
    //const size_t bufferSize = currentlyPlayingBufferSize;
    // This flag will get cleared if all goes well
    this->currentlyPlaying.error = true;
    if (autoTokenRefresh)
    {
        checkAndRefreshAccessToken();
    }

    int statusCode = makeGetRequest(command, this->_bearerToken);
    if (statusCode > 0)
    {
        skipHeaders();
    }

    if (statusCode == 200)
    {
        // Allocate DynamicJsonDocument
        //DynamicJsonDocument doc(bufferSize);

        // Parse JSON object
        DeserializationError error = deserializeJson(doc, *client);
        if (!error)
        {
            JsonObject item = doc["item"];
            JsonObject firstArtist = item["album"]["artists"][0];
            
			strncpy(this->currentlyPlaying.firstArtistName, (char *)firstArtist["name"].as<const char *>(), 63);
            strncpy(this->currentlyPlaying.firstArtistUri, (char *)firstArtist["uri"].as<const char *>(), 63);

            strncpy(this->currentlyPlaying.albumName, (char *)item["album"]["name"].as<const char *>(), 63);
            strncpy(this->currentlyPlaying.albumUri, (char *)item["album"]["uri"].as<const char *>(), 63);

            strncpy(this->currentlyPlaying.trackName, (char *)item["name"].as<const char *>(), 63);
            strncpy(this->currentlyPlaying.trackUri, (char *)item["uri"].as<const char *>(), 63);

            strncpy(this->currentlyPlaying.imgUrl, (char *)item["album"]["images"][0]["url"].as<char *>(), 64);

            this->currentlyPlaying.isPlaying = doc["is_playing"].as<bool>();

            this->currentlyPlaying.progressMs = doc["progress_ms"].as<long>();
            this->currentlyPlaying.duraitonMs = item["duration_ms"].as<long>();

            this->currentlyPlaying.error = false;
        }
        else
        {
            Serial.print(F("deserializeJson() failed with code "));
            Serial.println(error.c_str());
        }
	doc.clear();
    }
    closeClient();
    return &(this->currentlyPlaying);
}

PlayerDetails* ArduinoSpotify::getPlayerDetails(const char *market)
{
  _initDeviceStruct();
  memset(command, 0, 125*sizeof(char));
  strncpy(command, SPOTIFY_PLAYER_ENDPOINT, 124);
    if (market[0] != 0)
    {
        char marketBuff[11];
	memset(marketBuff, 0, 11*sizeof(char));
        sprintf(marketBuff, "?market=%s", market);
        strncat(command, marketBuff, (100-10-1));
    }

#ifdef SPOTIFY_DEBUG
    Serial.println(command);
#endif

    // Get from https://arduinojson.org/v6/assistant/
    //const size_t bufferSize = playerDetailsBufferSize;
//    PlayerDetails playerDetails;
    // This flag will get cleared if all goes well
    this->playerDetails.error = true;
    if (autoTokenRefresh)
    {
        checkAndRefreshAccessToken();
    }

    int statusCode = makeGetRequest(command, this->_bearerToken);
    if (statusCode > 0)
    {
        skipHeaders();
    }

    if (statusCode == 200)
    {
        // Allocate DynamicJsonDocument
        //DynamicJsonDocument doc(bufferSize);

        // Parse JSON object
        DeserializationError error = deserializeJson(doc, *client);
        if (!error)
        {
            JsonObject device = doc["device"];
            
            strncpy(this->playerDetails.device.id, (char *)device["id"].as<const char *>(), 40);
            strncpy(this->playerDetails.device.name, (char *)device["name"].as<const char *>(), 40);
            strncpy(this->playerDetails.device.type, (char *)device["type"].as<const char *>(), 19);
            this->playerDetails.device.isActive = device["is_active"].as<bool>();
            this->playerDetails.device.isPrivateSession = device["is_private_session"].as<bool>();
            this->playerDetails.device.isRestricted = device["is_restricted"].as<bool>();
            this->playerDetails.device.volumePercent = device["volume_percent"].as<int>();

            this->playerDetails.progressMs = doc["progress_ms"].as<long>();
            this->playerDetails.isPlaying = doc["is_playing"].as<bool>();

            this->playerDetails.shuffleState = doc["shuffle_state"].as<bool>();

            const char *repeat_state = doc["repeat_state"]; // "off"

            if (strncmp(repeat_state, "track", 5) == 0)
            {
                this->playerDetails.repeateState = repeat_track;
            }
            else if (strncmp(repeat_state, "context", 7) == 0)
            {
                this->playerDetails.repeateState = repeat_context;
            }
            else
            {
                this->playerDetails.repeateState = repeat_off;
            }

            this->playerDetails.error = false;
        }
        else
        {
			Serial.println("Pipan gick det");
            Serial.print(F("deserializeJson() failed with code "));
            Serial.println(error.c_str());
        }
	doc.clear();
    }
    closeClient();
    return &(this->playerDetails);
}

bool ArduinoSpotify::getImage(char *imageUrl, Stream *file)
{
#ifdef SPOTIFY_DEBUG
    Serial.print(F("Parsing image URL: "));
    Serial.println(imageUrl);
#endif

    uint8_t lengthOfString = strlen(imageUrl);

    // We are going to just assume https, that's all I've
    // seen and I can't imagine a company will go back
    // to http

    if (strncmp(imageUrl, "https://", 8) != 0)
    {
        Serial.print(F("Url not in expected format: "));
        Serial.println(imageUrl);
        Serial.println("(expected it to start with \"https://\")");
        return false;
    }

    uint8_t protocolLength = 8;

    char *pathStart = strchr(imageUrl + protocolLength, '/');
    uint8_t pathIndex = pathStart - imageUrl;
    uint8_t pathLength = lengthOfString - pathIndex;
    char path[pathLength + 1];
	memset(path, 0, (pathLength+1)*sizeof(char));
    strncpy(path, pathStart, pathLength);
//    path[pathLength] = '\0';

    uint8_t hostLength = pathIndex - protocolLength;
    char host[hostLength + 1];
    memset(host, 0, (hostLength+1)*sizeof(char));
    strncpy(host, imageUrl + protocolLength, hostLength);
//    host[hostLength] = '\0';

#ifdef SPOTIFY_DEBUG
    Serial.print(F("host: "));
    Serial.println(host);

    Serial.print(F("len:host:"));
    Serial.println(hostLength);

    Serial.print(F("path: "));
    Serial.println(path);

    Serial.print(F("len:path: "));
    Serial.println(strlen(path));
#endif

    bool status = false;
    int statusCode = makeGetRequest(path, NULL, "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8", host);
#ifdef SPOTIFY_DEBUG
    Serial.print(F("statusCode: "));
    Serial.println(statusCode);
#endif
    if (statusCode == 200)
    {
        int totalLength = getContentLength();
#ifdef SPOTIFY_DEBUG
        Serial.print(F("file length: "));
        Serial.println(totalLength);
#endif
        if (totalLength > 0)
        {
            skipHeaders(false);
            int remaining = totalLength;
            // This section of code is inspired but the "Web_Jpg"
            // example of TJpg_Decoder
            // https://github.com/Bodmer/TJpg_Decoder
            // -----------
            uint8_t buff[128] = {0};
            while (client->connected() && (remaining > 0 || remaining == -1))
            {
                // Get available data size
                size_t size = client->available();

                if (size)
                {
                    // Read up to 128 bytes
                    int c = client->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

                    // Write it to file
                    file->write(buff, c);

                    // Calculate remaining bytes
                    if (remaining > 0)
                    {
                        remaining -= c;
                    }
                }
                yield();
            }
// ---------
#ifdef SPOTIFY_DEBUG
            Serial.println(F("Finished getting image"));
#endif
            // probably?!
            status = true;
        }
    }

    closeClient();

    return status;
}

int ArduinoSpotify::getContentLength()
{

    if (client->find("Content-Length:"))
    {
        int contentLength = client->parseInt();
#ifdef SPOTIFY_DEBUG
        Serial.print(F("Content-Length: "));
        Serial.println(contentLength);
#endif
        return contentLength;
    }

    return -1;
}

void ArduinoSpotify::skipHeaders(bool tossUnexpectedForJSON)
{
    // Skip HTTP headers
    if (!client->find("\r\n\r\n"))
    {
        Serial.println(F("Invalid response"));
        return;
    }

    if (tossUnexpectedForJSON)
    {
        // Was getting stray characters between the headers and the body
        // This should toss them away
        while (client->available() && client->peek() != '{')
        {
            char c = 0;
            client->readBytes(&c, 1);
#ifdef SPOTIFY_DEBUG
            Serial.print(F("Tossing an unexpected character: "));
            Serial.println(c);
#endif
        }
    }
}

int ArduinoSpotify::getHttpStatusCode()
{
    char status[32] = {0};
    client->readBytesUntil('\r', status, sizeof(status));
#ifdef SPOTIFY_DEBUG
    Serial.print(F("Status: "));
    Serial.println(status);
#endif

    char *token;
    token = strtok(status, " "); // https://www.tutorialspoint.com/c_standard_library/c_function_strtok.htm

#ifdef SPOTIFY_DEBUG
    Serial.print(F("HTTP Version: "));
    Serial.println(token);
#endif

    if (token != NULL && (strcmp(token, "HTTP/1.0") == 0 || strcmp(token, "HTTP/1.1") == 0))
    {
        token = strtok(NULL, " ");
        if (token != NULL)
        {
#ifdef SPOTIFY_DEBUG
            Serial.print(F("Status Code: "));
            Serial.println(token);
#endif
            return atoi(token);
        }
    }

    return -1;
}

void ArduinoSpotify::parseError()
{
/*
    DynamicJsonDocument doc(1000);
    DeserializationError error = deserializeJson(doc, *client);
    if (!error)
    {
        Serial.print(F("getAuthToken error"));
        serializeJson(doc, Serial);
    }
    else
    {
        Serial.print(F("Could not parse error"));
    }
    doc.clear();
*/
}

void ArduinoSpotify::closeClient()
{
    if (client->connected())
    {
#ifdef SPOTIFY_DEBUG
        Serial.println(F("Closing client"));
#endif
        client->stop();
    }
}

SpotifyDevice* ArduinoSpotify::scanDevices()
{
  _initDeviceStruct();
  memset(command, 0, 125*sizeof(char));
  strncpy(command, "/v1/me/player/devices", 124);

#ifdef SPOTIFY_DEBUG
    Serial.println(command);
#endif

    // Get from https://arduinojson.org/v6/assistant/
    //const size_t bufferSize = currentlyPlayingBufferSize/5;
    // This flag will get cleared if all goes well
    if (autoTokenRefresh)
    {
        checkAndRefreshAccessToken();
    }

    int statusCode = makeGetRequest(command, this->_bearerToken);
    if (statusCode > 0)
    {
        skipHeaders();
    }

    if (statusCode == 200)
    {
        // Allocate DynamicJsonDocument
        //DynamicJsonDocument doc(bufferSize);

        // Parse JSON object
        DeserializationError error = deserializeJson(doc, *client);
        if (!error)
        {
          for (uint8_t i=0; i<1; i++) {
            strncpy(this->playerDetails.device.id, (char *)doc["devices"][i]["id"].as<const char *>(), 40);
	        this->playerDetails.device.isActive = doc["devices"][i]["id"]["is_active"].as<bool>();
            this->playerDetails.device.isPrivateSession = doc["devices"][i]["id"]["is_private_session"].as<bool>();
            this->playerDetails.device.isRestricted = doc["devices"][i]["id"]["is_restricted"].as<bool>();
            strncpy(playerDetails.device.name, (char *)doc["devices"][i]["name"].as<const char *>(), 40);
            strncpy(playerDetails.device.type, (char *)doc["devices"][i]["type"].as<const char *>(), 19);
            this->playerDetails.device.volumePercent = doc["devices"][i]["volume_percent"].as<int>();
          }
/*
struct SpotifyDevice
{
  char id[41];
  char name[41];
  char type[20];
  bool isActive;
  bool isRestricted;
  bool isPrivateSession;
  int volumePercent;
};
_devices[2];
*/
        }
        else
        {
            Serial.print(F("deserializeJson() failed with code "));
            Serial.println(error.c_str());
        }
	doc.clear();
    }
    closeClient();
    return &(this->playerDetails.device);
}

void
ArduinoSpotify::_initDeviceStruct() {
/*
struct SpotifyDevice
{
  char id[41];
  char name[41];
  char type[20];
  bool isActive;
  bool isRestricted;
  bool isPrivateSession;
  int volumePercent;
};

struct PlayerDetails
{
  SpotifyDevice device;
  long progressMs;
  bool isPlaying;
  RepeatOptions repeateState;
  bool shuffleState;
  bool error;
};
*/
  for (uint8_t i=0; i<1; i++) {
    memset(this->playerDetails.device.id, 0, 41*sizeof(char));
    memset(this->playerDetails.device.name, 0, 41*sizeof(char));
    memset(this->playerDetails.device.type, 0, 20*sizeof(char));
    this->playerDetails.device.isActive = false;
    this->playerDetails.device.isRestricted = true;
    this->playerDetails.device.isPrivateSession = true;
    this->playerDetails.device.volumePercent = 0;
  }
}

void
ArduinoSpotify::_initCurrentlyPlayingStruct()
{
  memset(this->currentlyPlaying.firstArtistName, 0, 64*sizeof(char));
  memset(this->currentlyPlaying.firstArtistUri, 0, 64*sizeof(char));
  memset(this->currentlyPlaying.albumName, 0, 64*sizeof(char));
  memset(this->currentlyPlaying.albumUri, 0, 64*sizeof(char));
  memset(this->currentlyPlaying.trackName, 0, 64*sizeof(char));
  memset(this->currentlyPlaying.trackUri, 0, 64*sizeof(char));
  memset(this->currentlyPlaying.imgUrl, 0, 65*sizeof(char));
  this->currentlyPlaying.isPlaying = 0;
  this->currentlyPlaying.progressMs = 0;
  this->currentlyPlaying.duraitonMs = 0;

  this->currentlyPlaying.error = true;
}
