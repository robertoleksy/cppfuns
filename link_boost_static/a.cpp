#include <boost/locale.hpp>
#include <iostream>
#include <locale>

using namespace std;

int main() {

	const string install_dir_share_locale("./");

	boost::locale::generator gen;

	// Specify location of dictionaries
	gen.add_messages_path(install_dir_share_locale);
	gen.add_messages_domain("galaxy42_main");
	std::string locale_name;
	try {
		locale_name = std::use_facet<boost::locale::info>(gen("")).name();
	} catch (const std::exception &e) {
		std::cerr << "Can not detect language, set default language" << "\n";
		locale_name = "en_US.UTF-8";
	}
	std::locale::global(gen(locale_name));
	//std::locale::global(gen("pl_PL.UTF-8")); // OK
	//std::locale::global(gen("Polish_Poland.UTF-8")); // not works
	std::cout.imbue(std::locale());
	std::cerr.imbue(std::locale());
	// Using boost::locale::gettext:
	std::cerr << std::string(80,'=') << std::endl << boost::locale::gettext("L_warning_work_in_progres") << std::endl << std::endl;
	std::cerr << boost::locale::gettext("L_program_is_pre_pre_alpha") << std::endl;
	std::cerr << boost::locale::gettext("L_program_is_copyrighted") << std::endl;
	std::cerr << std::endl;

}

