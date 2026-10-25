Untrusted input = an attacker-controlled format string / arguments to curl's printf family, or an
attacker-influenced URL/string reaching the same code. Realistic entry: `curl_maprintf`/`curl_mvaprintf`
from <curl/curl.h> (the mprintf surface), or `curl_url*` for URL parsing. A probe calls the public API
with the crafted input; fire = ASan abort inside libcurl. (No network needed.)
