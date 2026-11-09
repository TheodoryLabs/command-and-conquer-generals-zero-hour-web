/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: QuotedPrintable.cpp /////////////////////////////////////////////////////////
// Author: Matt Campbell, February 2002
// Description: Quoted-printable encode/decode
////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/QuotedPrintable.h"

#define MAGIC_CHAR '_'

// takes an integer and returns an ASCII representation
static char intToHexDigit(int num)
{
	if (num<0 || num >15) return '\0';
	if (num<10)
	{
		return '0' + num;
	}
	return 'A' + (num-10);
}

// convert an ASCII representation of a hex digit into the digit itself
static int hexDigitToInt(char c)
{
	if (c <= '9' && c >= '0') return (c - '0');
	if (c <= 'f' && c >= 'a') return (c - 'a' + 10);
	if (c <= 'F' && c >= 'A') return (c - 'A' + 10);
	return 0;
}

// Convert unicode strings into ascii quoted-printable strings
// GeneralsX @bugfix 09/11/2026 Walk WideChars, not bytes: wchar_t is 4 bytes on wasm/Linux, so the byte
// walk stopped at the first 00 00 and saved "Player12" as "P". Each char is written as its two
// UTF-16LE bytes, the format Windows (2-byte wchar_t) has always produced.
static void appendQuotedByte(char *dest, int &i, unsigned char b)
{
	if (!isalnum(b))
	{
		dest[i++] = MAGIC_CHAR;
		dest[i++] = intToHexDigit(b>>4);
		dest[i++] = intToHexDigit(b&0xf);
	}
	else
	{
		dest[i++] = b;
	}
}

AsciiString UnicodeStringToQuotedPrintable(UnicodeString original)
{
	static char dest[1024];
	const WideChar *src = original.str();
	int i=0;
	while ( *src != 0 && i<1018 )
	{
		unsigned int ch = (unsigned int)*src++;
		if (ch > 0xFFFF && ch <= 0x10FFFF)
		{
			// Beyond the BMP (emoji): write the UTF-16 surrogate pair Windows would store.
			if (i >= 1012)
				break; // both units won't fit; don't write half a pair
			ch -= 0x10000;
			const unsigned int hi = 0xD800 | (ch >> 10);
			appendQuotedByte(dest, i, (unsigned char)(hi & 0xff));
			appendQuotedByte(dest, i, (unsigned char)(hi >> 8));
			ch = 0xDC00 | (ch & 0x3FF);
		}
		appendQuotedByte(dest, i, (unsigned char)(ch & 0xff));
		appendQuotedByte(dest, i, (unsigned char)((ch >> 8) & 0xff));
	}
	dest[i] = '\0';

	return dest;
}

// Convert ascii strings into ascii quoted-printable strings
AsciiString AsciiStringToQuotedPrintable(AsciiString original)
{
	static char dest[1024];
	const unsigned char *src = reinterpret_cast<const unsigned char *>(original.str());
	int i=0;
	while ( src[0]!='\0' && i<1021 )
	{
		if (!isalnum(*src))
		{
			dest[i++] = MAGIC_CHAR;
			dest[i++] = intToHexDigit((*src)>>4);
			dest[i++] = intToHexDigit((*src)&0xf);
		}
		else
		{
			dest[i++] = *src;
		}
		src ++;
	}
	dest[i] = '\0';

	return dest;
}

// Convert ascii quoted-printable strings into unicode strings
// GeneralsX @bugfix 09/11/2026 Rebuild each WideChar from two UTF-16LE bytes (see above), whatever
// sizeof(WideChar) is.
UnicodeString QuotedPrintableToUnicodeString(AsciiString original)
{
	static WideChar dest[1024];
	int i=0;
	int byteIndex=0;
	unsigned int ch=0;
	const unsigned char *src = reinterpret_cast<const unsigned char *>(original.str());

	while (*src && i<1023)
	{
		unsigned char b;
		if (*src == MAGIC_CHAR)
		{
			if (src[1] == '\0')
			{
				// string ends with MAGIC_CHAR
				break;
			}
			b = hexDigitToInt(src[1]);
			src++;
			if (src[1] != '\0')
			{
				b = (unsigned char)((b<<4) | hexDigitToInt(src[1]));
				src++;
			}
		}
		else
		{
			b = *src;
		}
		src++;

		ch |= ((unsigned int)b) << (8*byteIndex);
		if (++byteIndex == 2)
		{
			if (ch == 0)
				break;
			// Where WideChar holds a full code point, join a UTF-16 surrogate pair back into one.
			if (sizeof(WideChar) > 2 && ch >= 0xDC00 && ch <= 0xDFFF && i > 0
				&& (unsigned int)dest[i-1] >= 0xD800 && (unsigned int)dest[i-1] <= 0xDBFF)
				dest[i-1] = (WideChar)(0x10000 + ((((unsigned int)dest[i-1]) - 0xD800) << 10) + (ch - 0xDC00));
			else
				dest[i++] = (WideChar)ch;
			ch = 0;
			byteIndex = 0;
		}
	}
	// An odd trailing byte is a char whose high byte was cut off.
	if (byteIndex == 1 && ch != 0 && i<1023)
		dest[i++] = (WideChar)ch;
	dest[i] = 0;

	return dest;
}

// Convert ascii quoted-printable strings into ascii strings
AsciiString QuotedPrintableToAsciiString(AsciiString original)
{
	static char dest[1024];
	int i=0;

	unsigned char *c = reinterpret_cast<unsigned char *>(dest);
	const unsigned char *src = reinterpret_cast<const unsigned char *>(original.str());

	while (*src && i<1023)
	{
		if (*src == MAGIC_CHAR)
		{
			if (src[1] == '\0')
			{
				// string ends with MAGIC_CHAR
				break;
			}
			*c = hexDigitToInt(src[1]);
			src++;
			if (src[1] != '\0')
			{
				*c = *c<<4;
				*c = *c | hexDigitToInt(src[1]);
				src++;
			}
		}
		else
		{
			*c = *src;
		}
		src++;
		c++;
	}

	*c = 0;

	return dest;
}

