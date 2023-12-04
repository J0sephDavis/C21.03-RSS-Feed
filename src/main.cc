#include <cstdlib>
#include <iostream>
#include <rapidxml.hpp>
#include <rapidxml_utils.hpp>

namespace rx = rapidxml;

int main(int argc, char** argv) {
	if (argc < 2) {
		std::cout << "Missing filepath\n";
		exit(EXIT_FAILURE);
	}
	rx::xml_document<> doc;
	std::cout << "argv[1]: " << argv[1] << "\n";
	rx::file<> xml_file(argv[1]);
	doc.parse<1>(xml_file.data());
	std::cout << "firstnode: " << doc.first_node()->first_attribute() << "\n";
	//
	exit(EXIT_SUCCESS);
}
