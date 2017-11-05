
// g++, clang++
// TODO same on mac clang?
#define ATTR_NODISCARD __attribute__((warn_unused_result))
// TODO msvc?  mingw?

ATTR_NODISCARD bool tryme() {	return 1; }

int main() {
	bool x = tryme();
	return x;
}

