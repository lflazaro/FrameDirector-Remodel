#include <stdint.h>

#include <json_object.h>
#include <json_tokener.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	const char *data1 = reinterpret_cast<const char *>(data);
	json_tokener *tok = json_tokener_new();
	json_object *obj = json_tokener_parse_ex(tok, data1, size);
	
}
