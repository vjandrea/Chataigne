/*
  ==============================================================================

	StreamDeckPlus.cpp
	Stream Deck + : 8 keys (4x2), 4 push dials, 800x100 touch strip

	Input report 0x01, byte 1 = event type:
	  0x00 keys  : byte 4+i = key i down
	  0x03 dials : byte 4 = 0x01 turn (bytes 5..8 = signed delta per dial)
	                        0x00 press (bytes 5..8 = 1 while held)
	  0x02 strip : byte 4 = 0x01 tap, 0x02 long press, 0x03 swipe
	               x at bytes 6-7 (LE, 0..799), swipe end x at bytes 10-11

	Unlike the Plus + XL, the Plus's keys and touch strip images are sent in
	their native orientation (no rotation needed), per Elgato's HID reference:
	https://docs.elgato.com/streamdeck/hid/stream-deck-plus

  ==============================================================================
*/

StreamDeckPlus::StreamDeckPlus(hid_device* device, String serialNumber) :
	StreamDeck(device, serialNumber, PLUS, 4, 2, false, 120, 4)
{
	imagePacketLength = 1024;
	imageHeaderLength = 8;
	numEncoders = 4;
	for (int i = 0; i < numEncoders; ++i) encoderStates.add(false);

	brightnessData.add(0x03, 0x08, 0xFF);
	brightnessData.insertMultiple(-1, 0x00, 29);

	resetData.add(0x03, 0x02);
	resetData.insertMultiple(-1, 0x00, 30);
}

void StreamDeckPlus::setBrightnessInternal(float brightness)
{
	brightnessData.set(2, (int)(brightness * 100));
}

void StreamDeckPlus::writeImageDataHeader(MemoryOutputStream& stream, int keyIndex, int partIndex, bool isLast, int bodyLength)
{
	stream.writeByte(0x02);
	stream.writeByte(0x07);
	stream.writeByte(keyIndex);
	stream.writeByte(isLast ? 1 : 0);
	stream.writeShort(bodyLength);
	stream.writeShort(partIndex);
}

void StreamDeckPlus::writeImageData(MemoryOutputStream& stream, Image& img)
{
	JPEGImageFormat format;
	format.setQuality(.9f);
	format.writeImageToStream(img, stream);
}

void StreamDeckPlus::setEncoderDisplays(const Array<EncoderDisplay>& displays)
{
	// 4 dials evenly spaced across the 800px strip
	const int spacing = stripWidth / numEncoders, firstCentre = spacing / 2, boxWidth = (int)(spacing * .9f);

	Image strip(Image::RGB, stripWidth, stripHeight, true);
	Graphics g(strip);
	g.fillAll(Colours::black);

	for (int i = 0; i < jmin(numEncoders, displays.size()); ++i)
	{
		const EncoderDisplay& d = displays.getReference(i);
		int centre = firstCentre + spacing * i;
		int x0 = jmax(0, centre - boxWidth / 2);
		int x1 = jmin(stripWidth, centre + boxWidth / 2);

		g.setColour(d.colour);
		g.fillRoundedRectangle(Rectangle<int>(x0, 2, x1 - x0, stripHeight - 5).toFloat(), 8);

		Colour textColour = d.colour.getPerceivedBrightness() > .6f ? Colours::black : Colours::white;
		int textWidth = x1 - x0 - 8;
		g.setColour(textColour);
		g.setFont(24);
		g.drawFittedText(d.label, x0 + 4, 8, textWidth, 28, Justification::centred, 1);
		g.setFont(30);
		g.drawFittedText(d.valueText, x0 + 4, 40, textWidth, 36, Justification::centred, 1);

		Rectangle<int> bar(x0 + 10, 82, x1 - x0 - 20, 8);
		g.setColour(Colours::black);
		g.fillRect(bar);
		g.setColour(textColour);
		g.fillRect(bar.withWidth(roundToInt(bar.getWidth() * jlimit(0.f, 1.f, d.normalizedValue))));
	}

	sendStripImage(strip);
}

void StreamDeckPlus::sendStripImage(const Image& img)
{
	if (device == nullptr || Engine::mainEngine->isClearing) return;

	MemoryOutputStream jpeg;
	JPEGImageFormat format;
	format.setQuality(.9f);
	format.writeImageToStream(img, jpeg);

	const int headerLength = 16;
	const int payload = imagePacketLength - headerLength;
	const int total = (int)jpeg.getDataSize();

	GenericScopedLock<SpinLock> lock(writeLock);
	for (int part = 0, offset = 0; offset < total; part++)
	{
		int body = jmin(total - offset, payload);

		MemoryOutputStream packet;
		packet.writeByte(0x02);
		packet.writeByte(0x0c);
		packet.writeShort(0);          // x
		packet.writeShort(0);          // y
		packet.writeShort(stripWidth);
		packet.writeShort(stripHeight);
		packet.writeByte(offset + body >= total ? 1 : 0);
		packet.writeShort(part);
		packet.writeShort(body);
		packet.writeByte(0);
		packet.write((uint8*)jpeg.getData() + offset, body);
		packet.writeRepeatedByte(0, imagePacketLength - packet.getDataSize());

		hid_write(device, (unsigned char*)packet.getData(), imagePacketLength);
		offset += body;
	}
}

void StreamDeckPlus::processInputReport(const unsigned char* data, int length)
{
	if (data[0] != 1 || length < 14) return;

	switch (data[1])
	{
	case 0x00:
		StreamDeck::processInputReport(data, length);
		break;

	case 0x03:
		for (int i = 0; i < numEncoders; ++i)
		{
			if (data[4] == 0x01)
			{
				int delta = (int8_t)data[5 + i];
				if (delta != 0) deviceListeners.call(&StreamDeckListener::streamDeckEncoderRotated, i, delta);
			}
			else if (data[4] == 0x00)
			{
				bool state = data[5 + i] > 0;
				if (encoderStates[i] == state) continue;
				encoderStates.set(i, state);
				deviceListeners.call(state ? &StreamDeckListener::streamDeckEncoderPressed : &StreamDeckListener::streamDeckEncoderReleased, i);
			}
		}
		break;

	case 0x02:
	{
		int x = data[6] | (data[7] << 8);
		if (data[4] == 0x01 || data[4] == 0x02)
		{
			deviceListeners.call(&StreamDeckListener::streamDeckStripTapped, jlimit(0.f, 1.f, x / (float)(stripWidth - 1)), data[4] == 0x02);
		}
		else if (data[4] == 0x03)
		{
			int endX = data[10] | (data[11] << 8);
			deviceListeners.call(&StreamDeckListener::streamDeckStripSwiped, endX > x);
		}
		break;
	}
	}
}
