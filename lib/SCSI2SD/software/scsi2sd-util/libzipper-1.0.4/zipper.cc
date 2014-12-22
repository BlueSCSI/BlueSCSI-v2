//	Copyright (C) 2011 Michael McMaster <michael@codesrc.com>
//
//	This file is part of libzipper.
//
//	libzipper is free software: you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	libzipper is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with libzipper.  If not, see <http://www.gnu.org/licenses/>.

#include "zipper.hh"
#include "split.hh"
#include <errno.h>
#include "strerror.hh"

#include <cassert>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <sstream>
#include <string>

#include "config.h"

using namespace zipper;

static std::string argv0;

static void usage()
{
	std::cerr <<
		"libzipper  Copyright (C) 2011 Michael McMaster <michael@codesrc.com>\n"
		"This program comes with ABSOLUTELY NO WARRANTY.\n"
		"This is free software, and you are welcome to redistribute it\n"
		"under certain conditions.\n\n" <<

		"Usage: \n" <<
			argv0 << " {zip|gzip} archive file [files...]\n" <<
			argv0 << " {unzip|gunzip} archive" << std::endl;
}

static WriterPtr
getWriter(const std::string& optionName)
{
	std::shared_ptr<FileWriter> writer;
	if (optionName == "-")
	{
		writer.reset(new FileWriter("stdout", 1, false));
	}
	else
	{
		writer.reset(new FileWriter(optionName, 0660));
	}
	return writer;
}

static ReaderPtr
getReader(const std::string& optionName)
{
	std::shared_ptr<FileReader> reader;
	if (optionName == "-")
	{
		reader.reset(new FileReader("stdin", 0, false));
	}
	else
	{
		reader.reset(new FileReader(optionName));
	}
	return reader;
}

static void
command_zip(const std::deque<std::string>& options)
{
	if (options.size() < 2)
	{
		usage();
		exit(EXIT_FAILURE);
	}

	WriterPtr writer(getWriter(options[0]));
	Compressor comp(Container_zip, writer);
	for (size_t i = 1; i < options.size(); ++i)
	{
		ReaderPtr reader(getReader(options[i]));
		comp.addFile(*reader);
	}
}

static void
command_gzip(const std::deque<std::string>& options)
{
	if (options.size() != 2)
	{
		usage();
		exit(EXIT_FAILURE);
	}

	WriterPtr writer(getWriter(options[0]));
	Compressor comp(Container_gzip, writer);
	for (size_t i = 1; i < options.size(); ++i)
	{
		ReaderPtr reader(getReader(options[i]));
		comp.addFile(*reader);
	}
}

static void
command_extract(const std::deque<std::string>& options)
{
	if (options.size() != 1)
	{
		usage();
		exit(EXIT_FAILURE);
	}

	ReaderPtr reader(getReader(options[0]));
	Decompressor decomp(reader);
	std::vector<CompressedFilePtr> entries(decomp.getEntries());
	for (size_t f = 0; f < entries.size(); ++f)
	{
		std::deque<std::string> path;
		split(
			entries[f]->getPath(),
			'/',
			std::back_insert_iterator<std::deque<std::string> >(path));
		path.pop_back(); // Remove extracted file.
		std::stringstream builtPath;
		for (std::deque<std::string>::iterator it(path.begin());
			it != path.end();
			++it)
		{
			builtPath << *it;
			int result(MKDIR(builtPath.str().c_str(), 0775));
			if (result != 0 && errno != EEXIST)
			{
				std::string errMsg(zipper::strerror(errno));

				std::stringstream message;
				message << "Could not create directory " <<
					"\"" <<builtPath.str() << "\": " << errMsg;
				throw IOException(message.str());
			}

			builtPath << '/';
		}
		FileWriter writer(
			entries[f]->getPath(), 0660, entries[f]->getModificationTime());
		entries[f]->decompress(writer);
	}
}

int main(int argc, char** argv)
{
	argv0 = argv[0];
	if (argc < 3)
	{
		usage();
		exit(EXIT_FAILURE);
	}

	std::deque<std::string> options;
	for (int i = 1; i < argc; ++i)
	{
		options.push_back(argv[i]);
	}

	std::string command(options[0]);
	options.pop_front();
	if (command == "zip")
	{
		command_zip(options);
	}
	else if (command == "gzip")
	{
		command_gzip(options);
	}
	else if (command == "gunzip" || command == "unzip")
	{
		command_extract(options);
	}
	else
	{
		usage();
		exit(EXIT_FAILURE);
	}

	return EXIT_SUCCESS;
}

