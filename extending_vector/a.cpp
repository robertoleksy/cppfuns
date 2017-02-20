// Copyrighted (C) 2016 by developers of this project, as gift-ware (as in lib allegro 4)

#include <vector>
#include <iostream>
#include <memory>
#include <cstddef>

template <typename T>
class myvec : public std::vector<T> {
	public:
		myvec(size_t n) : std::vector<T>(n) { }
		size_t size() const { return std::vector<T>::size() + 10000; }

		~myvec() { std::cout << "Destr of myvect" << std::endl; }
};


void use_vec(std::vector<int> & tab) {
	tab.at(2) = 42;
	tab[2] = 42;
	std::cout << __PRETTY_FUNCTION__ << " " << tab.size() << std::endl;
}

void use_myvec(myvec<int> & tab) {
	tab.at(2) = 42;
	tab[2] = 42;
	std::cout << __PRETTY_FUNCTION__ << " " << tab.size() << std::endl;
}

template<typename TV>
void use_TVvec(TV & tab) {
	tab.at(2) = 42;
	tab[2] = 42;
	std::cout << __PRETTY_FUNCTION__ << " " << tab.size() << std::endl;
}


int main() {
	{
		auto tab = std::make_shared< myvec<int> >(5);
		use_vec(*tab);
		use_myvec(*tab);
		use_TVvec(*tab);
	}
	std::cout << std::endl;

	// http://stackoverflow.com/questions/3899790/shared-ptr-magic
	{
		std::shared_ptr< std::vector<int> > tab = std::make_shared< myvec<int> >(5);
		use_vec(*tab);
	//	auto x = std::dynamic_pointer_cast< myvec<int> >(tab);
		//use_myvec(* std::dynamic_pointer_cast< std::shared_ptr<myvec<int>>(tab) );
//		use_myvec( static_cast<myvec<int>>( *tab ) );
	//	use_myvec( dynamic_cast<myvec<int> & >( *tab ) );
		use_TVvec(*tab);
	}
	std::cout << std::endl;

	{
		std::vector<int> * tab = new myvec<int> (5);
		use_vec(*tab);
	//	auto x = std::dynamic_pointer_cast< myvec<int> >(tab);
		//use_myvec(* std::dynamic_pointer_cast< std::shared_ptr<myvec<int>>(tab) );
//		use_myvec( static_cast<myvec<int>>( *tab ) );
	//	use_myvec( dynamic_cast<myvec<int> & >( *tab ) );
		use_TVvec(*tab);
		delete tab;
	}

}


