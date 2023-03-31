// inline_vector.cpp : Defines the entry point for the application.
//

#include "inline_vector.h"
#include "std_headers.h"

using namespace std;

int main()
{
    std::array<size_t, 32> data;
    inline_vector::inline_vector<size_t> v{data.data(), data.data(), data.data() + data.size()};

    for (size_t i = 0; i < v.capacity(); i++) {
        v.data()[i] = 0;
    }

    for (size_t i = 0; i < data.size(); i++)
        v.unchecked_emplace_back();
    

	return 0;
}
