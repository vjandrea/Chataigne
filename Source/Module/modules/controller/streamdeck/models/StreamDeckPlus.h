/*
  ==============================================================================

	StreamDeckPlus.h
	Stream Deck + : 8 keys (4x2), 4 push dials, 800x100 touch strip

  ==============================================================================
*/

#pragma once

class StreamDeckPlus :
	public StreamDeck
{
public:
	StreamDeckPlus(hid_device* device, String serialNumber);
	~StreamDeckPlus() {}

	static const int stripWidth = 800;
	static const int stripHeight = 100;

	Array<bool> encoderStates;

	void setBrightnessInternal(float brightness) override;

	void writeImageDataHeader(MemoryOutputStream& stream, int keyIndex, int partIndex, bool isLast, int bodyLength) override;
	void writeImageData(MemoryOutputStream& stream, Image& img) override;

	void setEncoderDisplays(const Array<EncoderDisplay>& displays) override;
	void sendStripImage(const Image& img);

	void processInputReport(const unsigned char* data, int length) override;
};
