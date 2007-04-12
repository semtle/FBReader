/*
 * FBReader -- electronic book reader
 * Copyright (C) 2004-2007 Nikolay Pultsin <geometer@mawhrin.net>
 * Copyright (C) 2005 Mikhail Sobolev <mss@mawhrin.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <algorithm>

#include <ZLXMLReader.h>
#include <ZLApplication.h>

#include "ProgramCollection.h"
#include "../FBOptions.h"

class ProgramCollectionBuilder : public ZLXMLReader {

public:
	ProgramCollectionBuilder(ProgramCollectionMap &collectionMap);
	~ProgramCollectionBuilder();

private:
	void startElementHandler(const char *tag, const char **attributes);	
	void endElementHandler(const char *tag);	

private:
	ProgramCollectionMap &myCollectionMap;
	shared_ptr<ProgramCollection> myCurrentCollection;
	shared_ptr<Program> myCurrentProgram;
};

static const std::string SECTION = "section";
static const std::string PROGRAM = "program";
static const std::string ACTION = "action";
static const std::string OPTION = "option";

ProgramCollectionBuilder::ProgramCollectionBuilder(ProgramCollectionMap &collectionMap) : myCollectionMap(collectionMap) {
}

ProgramCollectionBuilder::~ProgramCollectionBuilder() {
}

void ProgramCollectionBuilder::startElementHandler(const char *tag, const char **attributes) {
	if (SECTION == tag) {
		const char *name = attributeValue(attributes, "name");
		if (name != 0) {
			myCurrentCollection = myCollectionMap.myMap[name];
			if (myCurrentCollection.isNull()) {
				myCurrentCollection = new ProgramCollection(name);
				myCollectionMap.myMap[name] = myCurrentCollection;
			}
		}
	} else if (!myCurrentCollection.isNull() && (PROGRAM == tag)) {
		const char *name = attributeValue(attributes, "name");
		const char *protocol = attributeValue(attributes, "protocol");
		const char *testFile = attributeValue(attributes, "testFile");
		if ((name != 0) && (protocol != 0)) {
			shared_ptr<ZLCommunicator> communicator =
				ZLCommunicationManager::instance().createCommunicator(protocol, (testFile != 0) ? testFile : "");
			if (!communicator.isNull()) {
				std::string sName = name;
				myCurrentProgram = new Program(sName, communicator);
				myCurrentCollection->myNames.push_back(sName);
				myCurrentCollection->myPrograms[sName] = myCurrentProgram;
			}
		}
	} else if (!myCurrentProgram.isNull() && (ACTION == tag)) {
		const char *name = attributeValue(attributes, "name");
		if (name != 0) {
			ZLCommunicationManager::Data &data = myCurrentProgram->myCommandData[name];
			for (const char **it = attributes; (*it != 0) && (*(it + 1) != 0); it += 2) {
				if (*it != "name") {
					data[*it] = *(it + 1);
				}
			}
		}
	} else if (!myCurrentProgram.isNull() && (OPTION == tag)) {
		const char *name = attributeValue(attributes, "name");
		if (name != 0) {
			const char *defaultValue = attributeValue(attributes, "defaultValue");
			const char *displayName = attributeValue(attributes, "displayName");
			if ((defaultValue != 0) && (displayName != 0)) {
				const std::string sName = name;
				const std::string sDefaultValue = defaultValue;
				myCurrentProgram->myOptions.push_back(Program::OptionDescription(sName, sDefaultValue, displayName));
				myCurrentProgram->myDefaultValues[sName] = sDefaultValue;
			}
		}
	}
}

void ProgramCollectionBuilder::endElementHandler(const char *tag) {
	if (SECTION == tag) {
		if (!myCurrentCollection.isNull()) {
			const std::vector<std::string> &names = myCurrentCollection->names();
			ZLStringOption &nameOption = myCurrentCollection->CurrentNameOption;
			if (!names.empty() && (std::find(names.begin(), names.end(), nameOption.value()) == names.end())) {
				nameOption.setValue(names.front());
			}
		}
		myCurrentCollection = 0;
		myCurrentProgram = 0;
	} else if (PROGRAM == tag) {
		myCurrentProgram = 0;
	}
}

ProgramCollectionMap::ProgramCollectionMap() {
	ProgramCollectionBuilder builder(*this);
	builder.readDocument(ZLApplication::DefaultFilesPathPrefix() + "external.xml");
}

shared_ptr<ProgramCollection> ProgramCollectionMap::collection(const std::string &name) const {
	std::map<std::string,shared_ptr<ProgramCollection> >::const_iterator it = myMap.find(name);
	return (it != myMap.end()) ? it->second : 0;
}

ProgramCollection::ProgramCollection(const std::string &name) :
	EnableCollectionOption(ZLOption::CONFIG_CATEGORY, name, "Enabled", true),
	CurrentNameOption(ZLOption::CONFIG_CATEGORY, name, "Name", "") {
}

const std::vector<std::string> &ProgramCollection::names() const {
	return myNames;
}

shared_ptr<Program> ProgramCollection::program(const std::string &name) const {
	std::map<std::string,shared_ptr<Program> >::const_iterator it = myPrograms.find(name);
	return (it != myPrograms.end()) ? it->second : 0;
}

shared_ptr<Program> ProgramCollection::currentProgram() const {
	if (!EnableCollectionOption.value()) {
		return 0;
	}
	return program(CurrentNameOption.value());
}

Program::Program(const std::string &name, shared_ptr<ZLCommunicator> communicator) : myName(name), myCommunicator(communicator) {
}

void Program::run(const std::string &command, const std::string &parameter) const {
	if (!myCommunicator.isNull()) {
		std::map<std::string,ZLCommunicationManager::Data>::const_iterator it = myCommandData.find(command);
		if (it != myCommandData.end()) {
			ZLCommunicationManager::Data data = it->second;
			for (ZLCommunicationManager::Data::iterator jt = data.begin(); jt != data.end(); ++jt) {
				if (!jt->second.empty() && jt->second[0] == '%') {
					const std::string optionName = jt->second.substr(1);
					std::map<std::string,std::string>::const_iterator st = myDefaultValues.find(optionName);
					jt->second = ZLStringOption(
						FBOptions::EXTERNAL_CATEGORY,
						myName,
						optionName,
						(st != myDefaultValues.end()) ? st->second : "").value();
				}
			}
			shared_ptr<ZLMessageSender> sender = myCommunicator->createSender(data);
			if (!sender.isNull()) {
				sender->sendStringMessage(parameter);
			}
		}
	}
}

const std::vector<Program::OptionDescription> &Program::options() const {
	return myOptions;
}

Program::OptionDescription::OptionDescription(const std::string &name, const std::string &defaultValue, const std::string &displayName) : OptionName(name), DefaultValue(defaultValue), DisplayName(displayName) {
}