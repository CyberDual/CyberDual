#include <ImfBytesAttribute.h>
#include <vector>

using namespace Imf;

int main ()
{
    std::vector<unsigned char> big (64, 'A');
    std::vector<unsigned char> small (1, 'B');

    BytesAttribute target (big.size (), big.data ());
    BytesAttribute source (small.size (), small.data ());

    target.copyValueFrom (source);

    return 0;
}
