/*
 * FBReader -- electronic book reader
 * Copyright (C) 2005 Nikolay Pultsin <geometer@mawhrin.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "TxtBookReader.h"
#include "../../bookmodel/BookModel.h"

TxtBookReader::TxtBookReader(BookModel &model) : BookReader(model) {
}

void TxtBookReader::flushTextBufferToParagraph() {
	bool bufferIsEmpty = true;
	for (std::vector<std::string>::const_iterator it = myBuffer.begin(); bufferIsEmpty && (it != myBuffer.end()); it++) {
		for (std::string::const_iterator jt = it->begin(); jt != it->end(); jt++) {
			if (!isspace(*jt)) {
				bufferIsEmpty = false;
				break;
			}
		}
	}

	if (!bufferIsEmpty) {
		myConverter->convertBuffer(myBuffer);
		BookReader::flushTextBufferToParagraph();
		myLineFeedCounter = 0;
	}
}

bool TxtBookReader::characterDataHandler(const char *text, int len) {
	addDataToBuffer(text, len);
	return true;
}

bool TxtBookReader::newLineHandler() {
	flushTextBufferToParagraph();
	myLineFeedCounter++;
	if (myLineFeedCounter > 1) {
		endParagraph();
		beginParagraph();
	}
	return true;
}

void TxtBookReader::startDocumentHandler() {
	setMainTextModel();
	pushKind(REGULAR);
	beginParagraph();
	myLineFeedCounter = 1;
}

void TxtBookReader::endDocumentHandler() {
	endParagraph();
}
