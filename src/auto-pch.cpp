// *****************************************************************************
// 
// auto-pch.cpp
//
// Driver program for determining what files to add to a pch file.
//
// Copyright Chris Glover 2015
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// *****************************************************************************
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <iterator>
#include <regex>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/unordered/unordered_map.hpp>
#include <boost/graph/breadth_first_search.hpp>

typedef std::string include_vertex_t;

typedef boost::adjacency_list<
	boost::vecS, 
	boost::vecS,
	boost::bidirectionalS, 
    include_vertex_t,
	bool
> include_graph_t;

typedef boost::graph_traits<
	include_graph_t
>::vertex_descriptor include_vertex_descriptor_t;

typedef boost::unordered::unordered_map<
	std::string, 
	include_vertex_descriptor_t
> include_map_t;


// -----------------------------------------------------------------------------
//
void PrintUsage()
{
	std::cout << 

	"Usage: auto-pch <input-deps-file> <output-header-file> [regex-list-file]\n"
	"Where:\n"
	"input-deps-file:\n"
	"    File output by the compiler to indicate the headers used by the source.\n"
	"    This file can be optained from the compiler in the following ways:\n"
	"    gcc:  g++ -H -E -o /dev/null source.cpp 2> includes.txt\n"
	"    msvc: cl.exe /showIncludes /P source.cpp 1> nul 2> includes.txt\n"
	"\n"
	"output-header-file:\n"
	"    Target header file to generate.\n"
	"    This header should then be precompiled and force included into the\n"
	"    target source file.\n"
	"    To precompile:\n"
	"        gcc:  simply compile the file as if it were source file.\n"
	"        msvc: do nothing. File will be automatically precompiled\n"
	"              in the next step\n"
	"    The header should then be force included into the source file when\n"
	"    compiling that source file.\n"
	"    To force include:\n"
	"        gcc:  g++ -include <output-header-file>\n"
	"        msvc: cl /Yc<output-header-file> /Fp<output-header-file>.pch /FI<output-header-file>\n"
	"    Note that for MSVC, the force include and precompilation can all happen\n"
	"    in one step, whereas gcc requires an explicite precompilation step, but otherwise\n"
	"    doesn't need to be told any more about the precompiled file.\n"
	"regex-list-file [optional]:\n"
	"    Contains a line seperated list of regex expressions to compare the include files to.\n"
	"    This regex can be compared to the full file path in order to allow caching of system headers.\n"
	;
}

// -----------------------------------------------------------------------------
//
std::vector<std::regex> MaybeReadRegexFile(std::string const& file)
{
	std::vector<std::regex> regex_in;

	if(!file.empty())
	{
		std::ifstream ins(file);
		if(!ins.is_open())
		{
			throw std::runtime_error("Failed to open " + file + " for reading.");
		}	

		std::string current_line;
		while(std::getline(ins, current_line))
		{
			regex_in.emplace_back(std::move(current_line));
		}
	}

	return regex_in;
}

// -----------------------------------------------------------------------------
//
std::vector<std::string> MaybeReadExistingPchFile(std::string const& file)
{
	std::vector<std::string> lines_in;
	std::ifstream ins(file);
	if(ins.is_open())
	{
		std::string current_line;
		while(std::getline(ins, current_line))
		{
			lines_in.emplace_back(std::move(current_line));
		}
	}
	else
	{
		std::cout << "Failed to open " << file << " for reading.\n" 
				  << "File will be [re]created"
				  << std::endl
		;

	}	

	return lines_in;
}

// -----------------------------------------------------------------------------
//
void ReadGccDepsFileRecursive(
	include_graph_t& deps,
	include_vertex_descriptor_t const& parent,
	include_vertex_descriptor_t sibling,
	include_map_t& include_map,
	int depth, 
	int& line_number, 
	std::ifstream& ins)
{
	std::string file;
	while(ins)
	{
		int line_depth = 0;
		std::streampos line_start_pos = ins.tellg();

		while(ins && ins.peek() == '.')
		{
			++line_depth;
			ins.get();
		}

		if(line_depth == 0)
		{
			std::getline(ins, file);
			++line_number;
			continue;
		}

		if(line_depth <= depth)
		{
			ins.seekg(line_start_pos, std::ios::beg);
			return;
		}

		if(line_depth == (depth + 1))
		{
			ins.get();
			std::getline(ins, file);
			std::replace(
				file.begin(),
				file.end(),
				'\\',
				'/'
			);

			auto vert = include_map.find(file);
			if(vert == include_map.end())
			{
				sibling = boost::add_vertex(file, deps);
				vert = include_map.insert(std::make_pair(file, sibling)).first;	
			}
			boost::add_edge(parent, vert->second, deps);
			++line_number;
		}
		else
		{
			ins.seekg(line_start_pos, std::ios::beg);
			ReadGccDepsFileRecursive(deps, sibling, sibling, include_map, depth+1, line_number, ins);
		}
	}
}

// -----------------------------------------------------------------------------
//
void ReadMsvcDepsFileRecursive(
	include_graph_t& deps,
	include_vertex_descriptor_t const& parent,
	include_vertex_descriptor_t sibling,
	include_map_t& include_map,
	int depth, 
	int& line_number, 
	std::ifstream& ins)
{
	std::string file;
	while(ins)
	{
		char const* prefix = "Note: including file:";
		int line_depth = 0;
		std::streampos line_start_pos = ins.tellg();

		while(ins && ins.peek() == *prefix)
		{
			++prefix;
			ins.get();
		}

		while(ins && ins.peek() == ' ')
		{
			++line_depth;
			ins.get();
		}

		if(line_depth == 0)
		{
			std::getline(ins, file);
			++line_number;
			continue;
		}

		if(line_depth <= depth)
		{
			ins.seekg(line_start_pos, std::ios::beg);
			return;
		}

		if(line_depth == (depth + 1))
		{
			std::getline(ins, file);
			std::replace(
				file.begin(),
				file.end(),
				'\\',
				'/'
			);

			auto vert = include_map.find(file);
			if(vert == include_map.end())
			{
				sibling = boost::add_vertex(file, deps);
				vert = include_map.insert(std::make_pair(file, sibling)).first;	
			}
			boost::add_edge(parent, vert->second, deps);
			++line_number;
		}
		else
		{
			ins.seekg(line_start_pos, std::ios::beg);
			ReadMsvcDepsFileRecursive(deps, sibling, sibling, include_map, depth+1, line_number, ins);
		}
	}
}

// -----------------------------------------------------------------------------
//
include_graph_t ReadGccDepsFile(std::ifstream& ins)
{
	include_graph_t deps;
	include_vertex_descriptor_t root = add_vertex("", deps);
	include_map_t include_map;
	int line_number = 0;
	ReadGccDepsFileRecursive(deps, root, root, include_map, 0, line_number, ins);
	return deps;
}

// -----------------------------------------------------------------------------
//
include_graph_t ReadMsvcDepsFile(std::ifstream& ins)
{
	include_graph_t deps;
	include_vertex_descriptor_t root = add_vertex("", deps);
	include_map_t include_map;
	int line_number = 0;
	ReadMsvcDepsFileRecursive(deps, root, root, include_map, 0, line_number, ins);
	return deps;
}

// -----------------------------------------------------------------------------
//
include_graph_t ReadDepsFile(std::string const& file)
{
	std::ifstream ins(file);
	if(!ins.is_open())
	{
		throw std::runtime_error("Failed to open " + file + " for reading.");
	}

	if(ins.peek() == '.')
	{
		return ReadGccDepsFile(ins);
	}
	else
	{
		return ReadMsvcDepsFile(ins);
	}
}

// -----------------------------------------------------------------------------
//
struct vertex_filter : public boost::default_dfs_visitor
{
	vertex_filter(
		std::vector<std::regex> const& filter,
		std::vector<std::string>& keepers,
		std::size_t num_verts)
		: filter_(filter)
		, keepers_(keepers)
	{
		included_.resize(num_verts, false);
	}

	template <class Edge, class Graph>
	void examine_edge(Edge const& e, Graph const& g) 
	{
		std::string const& target_f = g[e.m_target];
		std::string const& source_f = g[e.m_source];

		if(included_[e.m_source])
		{
			included_[e.m_target] = true;
			return;
		}

		bool match = std::any_of(filter_.begin(), filter_.end(),
			[&target_f](std::regex const& test)
			{
				return std::regex_match(target_f.begin(), target_f.end(), test);
			}
		);

		if(match)
		{
			included_[e.m_target] = true;
			keepers_.emplace_back(target_f);
		}
	}

	std::vector<std::regex> const& filter_;
	std::vector<std::string>& keepers_;
	std::vector<bool> included_;
};

// -----------------------------------------------------------------------------
//
std::vector<std::string> ComputeIncludeFiles(
	std::vector<std::regex> const& regex, 
	include_graph_t const& deps)
{
	std::vector<std::string> keepers;
	vertex_filter filter(regex, keepers, boost::num_vertices(deps));
	boost::depth_first_search(deps, boost::visitor(filter));
	return keepers;
}

// -----------------------------------------------------------------------------
//
int main(int argc, char** argv)
{
	if(argc < 3)
	{
		PrintUsage();
		return 1;
	}

	std::string input_deps_file = argv[1];
	std::string output_header_file = argv[2];
	std::string options_file_name;
	if(argc > 3)
	{
		options_file_name = argv[3];
	}

	std::vector<std::regex> regex_in;
	std::vector<std::string> lines_in;
	include_graph_t deps_in;

	try
	{
		regex_in = MaybeReadRegexFile(options_file_name);
		lines_in = MaybeReadExistingPchFile(output_header_file);
		deps_in = ReadDepsFile(input_deps_file);
	}	
	catch(std::runtime_error& e)
	{
		std::cout << e.what() << std::endl;
		PrintUsage();
		return 1;
	}

	std::vector<std::string> lines_out = ComputeIncludeFiles(regex_in, deps_in);

	// Don't change the file if not necessary.
	if(lines_in != lines_out)
	{
		// Outout the filtered lines to the target pch file.
		std::ofstream outs(output_header_file);
		if(!outs.good())
		{
			std::cerr << "Failed to open " << output_header_file << " for writing." 
					  << std::endl
			;

			PrintUsage();
			return 1;
		}

		std::transform(
			lines_out.begin(),
			lines_out.end(),
			std::ostream_iterator<std::string>(outs, "\n"),
			[](std::string const& file)
			{
				return "#include \"" + file + "\""; 
			}
		);
	}

	return 0;
}