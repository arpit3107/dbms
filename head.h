#ifndef aaudio
#define aaudio

#include <iostream>
#include <vector>
#include <assert.h>
#include <string>
#include <fstream>
#include <unordered_map>
#include <iterator>
#include <algorithm>

using namespace std;

enum class WavFileFormat
{
    Error,
    NotLoaded,
    Wave
};
template <class T>
class WavFile
{
public:
    typedef vector<vector<T> > WavBuffer;
    WavFile();
    bool load (string filePath);
    uint32_t getSampleRate() const;
    bool save (string filePath, WavFileFormat format = WavFileFormat::Wave);
    int getBitDepth() const;
    int getNumChannels() const;
    void IncVol (float times);
    void DecVol (float times);
    int getNumSamplesPerChannel() const;
    double getLengthInSeconds() const;
    WavBuffer samples;
    
private:
    enum class Endianness
    {
        LittleEndian,
        BigEndian
    };
    WavFileFormat determineWavFileFormat (vector<uint8_t>& fileData);
    bool decodeWaveFile (vector<uint8_t>& fileData);
    bool saveToWaveFile (string filePath);
    void clearWavBuffer();
    int32_t fourBytesToInt (vector<uint8_t>& source, int startIndex, Endianness endianness = Endianness::LittleEndian);
    int16_t twoBytesToInt (vector<uint8_t>& source, int startIndex, Endianness endianness = Endianness::LittleEndian);
    int getIndexOfString (vector<uint8_t>& source, string s);
    T sixteenBitIntToSample (int16_t sample);
    int16_t sampleToSixteenBitInt (T sample);
    uint8_t sampleToSingleByte (T sample);
    T singleByteToSample (uint8_t sample);
    bool tenByteMatch (vector<uint8_t>& v1, int startIndex1, vector<uint8_t>& v2, int startIndex2);
    T clamp (T v1, T minValue, T maxValue);
    void addStringToFileData (vector<uint8_t>& fileData, string s);
    void addInt32ToFileData (vector<uint8_t>& fileData, int32_t i, Endianness endianness = Endianness::LittleEndian);
    void addInt16ToFileData (vector<uint8_t>& fileData, int16_t i, Endianness endianness = Endianness::LittleEndian);
    bool writeDataToFile (vector<uint8_t>& fileData, string filePath);
    void reportError (string errorMessage);
    WavFileFormat wavFileFormat;
    uint32_t sampleRate;
    int bitDepth;
};

template <class T>
WavFile<T>::WavFile()
{
    bitDepth = 16;
    sampleRate = 44100;
    samples.resize (1);
    samples[0].resize (0);
    wavFileFormat = WavFileFormat::NotLoaded;
}
template <class T>
uint32_t WavFile<T>::getSampleRate() const
{
    return sampleRate;
}
template <class T>
int WavFile<T>::getNumChannels() const
{
    return (int)samples.size();
}
template <class T>
int WavFile<T>::getBitDepth() const
{
    return bitDepth;
}
template <class T>
int WavFile<T>::getNumSamplesPerChannel() const
{
    if (samples.size() > 0)
        return (int) samples[0].size();
    else
        return 0;
}

template<class T>
void WavFile<T>::IncVol (float times)
{
    for (size_t i = 0; i < samples.size();i++)
    {
        for(size_t j=0; j < getNumSamplesPerChannel();j++)
        {
            samples[i][j]+=(T)(samples[i][j] * times);
        }
    }

}
template<class T>
void WavFile<T>::DecVol (float times)
{
    for (size_t i = 0; i < samples.size();i++)
    {
        for(size_t j=0; j < getNumSamplesPerChannel();j++)
        {
           samples[i][j]-=(T)(samples[i][j] * times);
        }
    }
}
template <class T>
bool WavFile<T>::load (string filePath)
{
    ifstream file (filePath, ios::binary);
    if (! file.good())
    {
        reportError ("ERROR: File doesn't exist or otherwise can't load file\n"  + filePath);
        return false;
    }
    file.unsetf (ios::skipws);
    istream_iterator<uint8_t> begin (file), end;
    vector<uint8_t> fileData (begin, end);
    string header (fileData.begin(), fileData.begin() + 4);
    if (header == "RIFF")
    {
        return decodeWaveFile (fileData);
    }
    else
    {
        reportError ("Audio File Type: Error");
        return false;
    }
}
template <class T>
bool WavFile<T>::decodeWaveFile (vector<uint8_t>& fileData)
{
    string headerChunkID (fileData.begin(), fileData.begin() + 4);
    string format (fileData.begin() + 8, fileData.begin() + 12);
    int indexOfDataChunk = getIndexOfString (fileData, "data");
    int indexOfFormatChunk = getIndexOfString (fileData, "fmt");
    if (indexOfDataChunk == -1 || indexOfFormatChunk == -1 || headerChunkID != "RIFF" || format != "WAVE")
    {
        reportError ("ERROR: this doesn't seem to be a valid .WAV file");
        return false;
    }
    int f = indexOfFormatChunk;
    string formatChunkID (fileData.begin() + f, fileData.begin() + f + 4);
    int16_t audioFormat = twoBytesToInt (fileData, f + 8);
    int16_t numChannels = twoBytesToInt (fileData, f + 10);
    sampleRate = (uint32_t) fourBytesToInt (fileData, f + 12);
    int32_t numBytesPerSecond = fourBytesToInt (fileData, f + 16);
    int16_t numBytesPerBlock = twoBytesToInt (fileData, f + 20);
    bitDepth = (int) twoBytesToInt (fileData, f + 22);
    int numBytesPerSample = bitDepth / 8;
    if (audioFormat != 1)
    {
        reportError ("ERROR: this is a compressed .WAV file and this library does not support decoding them at present");
        return false;
    }
    if (numChannels < 1 ||numChannels > 2)
    {
        reportError ("ERROR: this WAV file seems to be neither mono nor stereo (perhaps multi-track, or corrupted?)");
        return false;
    }
    if ((numBytesPerSecond != (numChannels * sampleRate * bitDepth) / 8) || (numBytesPerBlock != (numChannels * numBytesPerSample)))
    {
        reportError ("ERROR: the header data in this WAV file seems to be inconsistent");
        return false;
    }
    if (bitDepth != 8 && bitDepth != 16 && bitDepth != 24)
    {
        reportError ("ERROR: this file has a bit depth that is not 8, 16 or 24 bits");
        return false;
    }
    int d = indexOfDataChunk;
    string dataChunkID (fileData.begin() + d, fileData.begin() + d + 4);
    int32_t dataChunkSize = fourBytesToInt (fileData, d + 4);
    
    int numSamples = dataChunkSize / (numChannels * bitDepth / 8);
    int samplesStartIndex = indexOfDataChunk + 8;
    
    clearWavBuffer();
    samples.resize (numChannels);
    
    for (int i = 0; i < numSamples; i++)
    {
        for (int channel = 0; channel < numChannels; channel++)
        {
            int sampleIndex = samplesStartIndex + (numBytesPerBlock * i) + channel * numBytesPerSample;
            
            if (bitDepth == 8)
            {
                T sample = singleByteToSample (fileData[sampleIndex]);
                samples[channel].push_back (sample);
            }
            else if (bitDepth == 16)
            {
                int16_t sampleAsInt = twoBytesToInt (fileData, sampleIndex);
                T sample = sixteenBitIntToSample (sampleAsInt);
                samples[channel].push_back (sample);
            }
            else if (bitDepth == 24)
            {
                int32_t sampleAsInt = 0;
                sampleAsInt = (fileData[sampleIndex + 2] << 16) | (fileData[sampleIndex + 1] << 8) | fileData[sampleIndex];
                
                if (sampleAsInt & 0x800000)
                    sampleAsInt = sampleAsInt | ~0xFFFFFF; 

                T sample = (T)sampleAsInt / (T)8388608.;
                samples[channel].push_back (sample);
            }
            else
            {
                assert (false);
            }
        }
    }

    return true;
}

template <class T>
bool WavFile<T>::tenByteMatch (vector<uint8_t>& v1, int startIndex1, vector<uint8_t>& v2, int startIndex2)
{
    for (int i = 0; i < 10; i++)
    {
        if (v1[startIndex1 + i] != v2[startIndex2 + i])
            return false;
    }
    
    return true;
}


template <class T>
bool WavFile<T>::save (string filePath, WavFileFormat format)
{
    if (format == WavFileFormat::Wave)
    {
        return saveToWaveFile (filePath);
    }
    
    return false;
}

template <class T>
bool WavFile<T>::saveToWaveFile (string filePath)
{
    vector<uint8_t> fileData;
    int32_t dataChunkSize = getNumSamplesPerChannel() * (getNumChannels() * bitDepth / 8);
    addStringToFileData (fileData, "RIFF");
    int32_t fileSizeInBytes = 4 + 24 + 8 + dataChunkSize;
    addInt32ToFileData (fileData, fileSizeInBytes);
    addStringToFileData (fileData, "WAVE");
    addStringToFileData (fileData, "fmt ");
    addInt32ToFileData (fileData, 16);
    addInt16ToFileData (fileData, 1); 
    addInt16ToFileData (fileData, (int16_t)getNumChannels());
    addInt32ToFileData (fileData, (int32_t)sampleRate);
    int32_t numBytesPerSecond = (int32_t) ((getNumChannels() * sampleRate * bitDepth) / 8);
    addInt32ToFileData (fileData, numBytesPerSecond);
    int16_t numBytesPerBlock = getNumChannels() * (bitDepth / 8);
    addInt16ToFileData (fileData, numBytesPerBlock);
    addInt16ToFileData (fileData, (int16_t)bitDepth);
    addStringToFileData (fileData, "data");
    addInt32ToFileData (fileData, dataChunkSize);
    for (int i = 0; i < getNumSamplesPerChannel(); i++)
    {
        for (int channel = 0; channel < getNumChannels(); channel++)
        {
            if (bitDepth == 8)
            {
                uint8_t byte = sampleToSingleByte (samples[channel][i]);
                fileData.push_back (byte);
            }
            else if (bitDepth == 16)
            {
                int16_t sampleAsInt = sampleToSixteenBitInt (samples[channel][i]);
                addInt16ToFileData (fileData, sampleAsInt);
            }
            else if (bitDepth == 24)
            {
                int32_t sampleAsIntAgain = (int32_t) (samples[channel][i] * (T)8388608.);
                
                uint8_t bytes[3];
                bytes[2] = (uint8_t) (sampleAsIntAgain >> 16) & 0xFF;
                bytes[1] = (uint8_t) (sampleAsIntAgain >>  8) & 0xFF;
                bytes[0] = (uint8_t) sampleAsIntAgain & 0xFF;
                
                fileData.push_back (bytes[0]);
                fileData.push_back (bytes[1]);
                fileData.push_back (bytes[2]);
            }
            else
            {
                assert (false && "Trying to write a file with unsupported bit depth");
                return false;
            }
        }
    }
    if (fileSizeInBytes != static_cast<int32_t> (fileData.size() - 8) || dataChunkSize != (getNumSamplesPerChannel() * getNumChannels() * (bitDepth / 8)))
    {
        reportError ("ERROR: couldn't save file to " + filePath);
        return false;
    }
    return writeDataToFile (fileData, filePath);
}

template <class T>
bool WavFile<T>::writeDataToFile (vector<uint8_t>& fileData, string filePath)
{
    ofstream outputFile (filePath, ios::binary);
    
    if (outputFile.is_open())
    {
        for (size_t i = 0; i < fileData.size(); i++)
        {
            char value = (char) fileData[i];
            outputFile.write (&value, sizeof (char));
        }
        
        outputFile.close();
        
        return true;
    }
    
    return false;
}

template <class T>
void WavFile<T>::addStringToFileData (vector<uint8_t>& fileData, string s)
{
    for (size_t i = 0; i < s.length();i++)
        fileData.push_back ((uint8_t) s[i]);
}

template <class T>
void WavFile<T>::addInt32ToFileData (vector<uint8_t>& fileData, int32_t i, Endianness endianness)
{
    uint8_t bytes[4];
    
    if (endianness == Endianness::LittleEndian)
    {
        bytes[3] = (i >> 24) & 0xFF;
        bytes[2] = (i >> 16) & 0xFF;
        bytes[1] = (i >> 8) & 0xFF;
        bytes[0] = i & 0xFF;
    }
    else
    {
        bytes[0] = (i >> 24) & 0xFF;
        bytes[1] = (i >> 16) & 0xFF;
        bytes[2] = (i >> 8) & 0xFF;
        bytes[3] = i & 0xFF;
    }
    
    for (int i = 0; i < 4; i++)
        fileData.push_back (bytes[i]);
}

template <class T>
void WavFile<T>::addInt16ToFileData (vector<uint8_t>& fileData, int16_t i, Endianness endianness)
{
    uint8_t bytes[2];
    
    if (endianness == Endianness::LittleEndian)
    {
        bytes[1] = (i >> 8) & 0xFF;
        bytes[0] = i & 0xFF;
    }
    else
    {
        bytes[0] = (i >> 8) & 0xFF;
        bytes[1] = i & 0xFF;
    }
    
    fileData.push_back (bytes[0]);
    fileData.push_back (bytes[1]);
}

template <class T>
void WavFile<T>::clearWavBuffer()
{
    for (size_t i = 0; i < samples.size();i++)
    {
        samples[i].clear();
    }
    
    samples.clear();
}

template <class T>
int32_t WavFile<T>::fourBytesToInt (vector<uint8_t>& source, int startIndex, Endianness endianness)
{
    int32_t result;
    
    if (endianness == Endianness::LittleEndian)
        result = (source[startIndex + 3] << 24) | (source[startIndex + 2] << 16) | (source[startIndex + 1] << 8) | source[startIndex];
    else
        result = (source[startIndex] << 24) | (source[startIndex + 1] << 16) | (source[startIndex + 2] << 8) | source[startIndex + 3];
    
    return result;
}

template <class T>
int16_t WavFile<T>::twoBytesToInt (vector<uint8_t>& source, int startIndex, Endianness endianness)
{
    int16_t result;
    
    if (endianness == Endianness::LittleEndian)
        result = (source[startIndex + 1] << 8) | source[startIndex];
    else
        result = (source[startIndex] << 8) | source[startIndex + 1];
    
    return result;
}

template <class T>
int WavFile<T>::getIndexOfString (vector<uint8_t>& source, string stringToSearchFor)
{
    int index = -1;
    int stringLength = (int)stringToSearchFor.length();
    
    for (size_t i = 0; i < source.size() - stringLength;i++)
    {
        string section (source.begin() + i, source.begin() + i + stringLength);
        
        if (section == stringToSearchFor)
        {
            index = static_cast<int> (i);
            break;
        }
    }
    
    return index;
}

template <class T>
T WavFile<T>::sixteenBitIntToSample (int16_t sample)
{
    return static_cast<T> (sample) / static_cast<T> (32768.);
}

template <class T>
int16_t WavFile<T>::sampleToSixteenBitInt (T sample)
{
    sample = clamp (sample, -1., 1.);
    return static_cast<int16_t> (sample * 32767.);
}

template <class T>
uint8_t WavFile<T>::sampleToSingleByte (T sample)
{
    sample = clamp (sample, -1., 1.);
    sample = (sample + 1.) / 2.;
    return static_cast<uint8_t> (sample * 255.);
}

template <class T>
T WavFile<T>::singleByteToSample (uint8_t sample)
{
    return static_cast<T> (sample - 128) / static_cast<T> (128.);
}

template <class T>
T WavFile<T>::clamp (T value, T minValue, T maxValue)
{
    value = min (value, maxValue);
    value = max (value, minValue);
    return value;
}

template <class T>
void WavFile<T>::reportError (string errorMessage)
{
    cout << errorMessage << endl;
}

#endif