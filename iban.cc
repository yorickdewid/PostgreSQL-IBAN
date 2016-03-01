extern "C"
{
#include <postgres.h>
#include <string.h>
#include <fmgr.h>
#include <utils/builtins.h>
}

#include <iostream>
#include <sstream>
#include <string>
#include <cctype>
#include <regex>

#include "specification.h"
#include "validate.h"

extern "C"
{
#ifdef PG_MODULE_MAGIC
	PG_MODULE_MAGIC;
#endif

	extern Datum iban_validate(PG_FUNCTION_ARGS);
}

void Validate::addSpecification(Specification* specPtr) {
	this->specifications[specPtr->countryCode] = specPtr;
}

/**
* Calculates the MOD 97 10 of the passed IBAN as specified in ISO7064.
*
* @param string iban
* @returns {bool}
*/
bool iso7064Mod97_10(std::string iBan) {
	std::rotate(iBan.begin(), iBan.begin() + 4, iBan.end());
	std::string numberstring;//will contain the letter substitutions
	for (const auto& c : iBan) {
		if (std::isdigit(c))
			numberstring += c;
		if (std::isupper(c))
			numberstring += std::to_string(static_cast<int>(c) - 55);
	}
	//implements a stepwise check for mod 97 in chunks of 9 at the first time
	// , then in chunks of seven prepended by the last mod 97 operation converted
	//to a string
	int segstart = 0;
	int step = 9;
	std::string prepended;
	long number = 0;
	while (segstart  < numberstring.length() - step) {
		number = std::stol(prepended + numberstring.substr(segstart, step));
		int remainder = number % 97;
		prepended = std::to_string(remainder);
		if (remainder < 10)
			prepended = "0" + prepended;
		segstart = segstart + step;
		step = 7;
	}
	number = std::stol(prepended + numberstring.substr(segstart));
	return (number % 97 == 1);
}

/**
* parse the bban structure used to configure each iban specification and returns a matching regular expression.
* a structure is composed of blocks of 3 characters (one letter and 2 digits). each block represents
* a logical group in the typical representation of the bban. for each group, the letter indicates which characters
* are allowed in this group and the following 2-digits number tells the length of the group.
*
* @param {string} structure the structure to parse
* @returns {regexp}
*/
std::regex parseStructure(std::string structure) {
	std::smatch match;

	std::string::const_iterator text_iter = structure.cbegin();
	std::ostringstream result;

	while (std::regex_search(text_iter, structure.cend(), match,
	                         std::regex("(.{3})"))) {
		std::string format;
		char pattern = match[0].str()[0];
		int repeats = std::stoi((match[0].str().substr(1)));

		/* Parse each structure block (1-char + 2-digits) */
		switch (pattern) {
			case 'A': format = "0-9A-ZA-Z"; break;
			case 'B': format = "0-9A-Z"; break;
			case 'C': format = "A-ZA-Z"; break;
			case 'F': format = "0-9"; break;
			case 'L': format = "A-Z"; break;
			case 'U': format = "A-Z"; break;
			case 'W': format = "0-9A-Z"; break;
		}

		result << "([" << format << "]{" << repeats << "})";

		text_iter = match[0].second;
	}

	std::string regex = "^" + result.str() + "$";
	return std::regex(regex.c_str());
}

bool Validate::isValid(std::string arg) {
	Specification* spec = new Specification(arg);

	/* Convert uppercase */
	std::transform(spec->example.begin(), spec->example.end(),
	               spec->example.begin(), toupper);

	/* Match on country */
	spec->countryCode = spec->example.substr(0, 2);
	spec->length = spec->example.length();
	Specification* specFound = this->specifications[spec->countryCode];
	if (!specFound)
		return false;

	/* Test accountnumber */
	std::string shortened = spec->example.substr(4, spec->example.length());
	bool result = specFound->length == spec->length
	              && specFound->countryCode.compare(spec->countryCode) == 0
	              && std::regex_match(shortened, parseStructure(specFound->structure))
	              && iso7064Mod97_10(spec->example);

	delete spec;
	return result;
}

Validate::~Validate() {
	for (auto it = specifications.begin(); it != specifications.end(); it++) {
		delete it->second;
	}
}

Validate::Validate() {
	addSpecification(new Specification(
	                     "AD", 24, "F04F04A12",
	                     "AD1200012030200359100100"));
	addSpecification(new Specification(
	                     "AE", 23, "F03F16",
	                     "AE070331234567890123456"));
	addSpecification(new Specification(
	                     "AL", 28, "F08A16",
	                     "AL47212110090000000235698741"));
	addSpecification(new Specification(
	                     "AT", 20, "F05F11",
	                     "AT611904300234573201"));
	addSpecification(new Specification(
	                     "AZ", 28, "U04A20",
	                     "AZ21NABZ00000000137010001944"));
	addSpecification(new Specification(
	                     "BA", 20, "F03F03F08F02",
	                     "BA391290079401028494"));
	addSpecification(new Specification(
	                     "BE", 16, "F03F07F02",
	                     "BE68539007547034"));
	addSpecification(new Specification(
	                     "BG", 22, "U04F04F02A08",
	                     "BG80BNBG96611020345678"));
	addSpecification(new Specification(
	                     "BH", 22, "U04A14",
	                     "BH67BMAG00001299123456"));
	addSpecification(new Specification(
	                     "BR", 29, "F08F05F10U01A01",
	                     "BR9700360305000010009795493P1"));
	addSpecification(new Specification(
	                     "CH", 21, "F05A12",
	                     "CH9300762011623852957"));
	addSpecification(new Specification(
	                     "CR", 21, "F03F14",
	                     "CR0515202001026284066"));
	addSpecification(new Specification(
	                     "CY", 28, "F03F05A16",
	                     "CY17002001280000001200527600"));
	addSpecification(new Specification(
	                     "CZ", 24, "F04F06F10",
	                     "CZ6508000000192000145399"));
	addSpecification(new Specification(
	                     "DE", 22, "F08F10",
	                     "DE89370400440532013000"));
	addSpecification(new Specification(
	                     "DK", 18, "F04F09F01",
	                     "DK5000400440116243"));
	addSpecification(new Specification(
	                     "DO", 28, "U04F20",
	                     "DO28BAGR00000001212453611324"));
	addSpecification(new Specification(
	                     "EE", 20, "F02F02F11F01",
	                     "EE382200221020145685"));
	addSpecification(new Specification(
	                     "ES", 24, "F04F04F01F01F10",
	                     "ES9121000418450200051332"));
	addSpecification(new Specification(
	                     "FI", 18, "F06F07F01",
	                     "FI2112345600000785"));
	addSpecification(new Specification(
	                     "FO", 18, "F04F09F01",
	                     "FO6264600001631634"));
	addSpecification(new Specification(
	                     "FR", 27, "F05F05A11F02",
	                     "FR1420041010050500013M02606"));
	addSpecification(new Specification(
	                     "GB", 22, "U04F06F08",
	                     "GB29NWBK60161331926819"));
	addSpecification(new Specification(
	                     "GE", 22, "U02F16",
	                     "GE29NB0000000101904917"));
	addSpecification(new Specification(
	                     "GI", 23, "U04A15",
	                     "GI75NWBK000000007099453"));
	addSpecification(new Specification(
	                     "GL", 18, "F04F09F01",
	                     "GL8964710001000206"));
	addSpecification(new Specification(
	                     "GR", 27, "F03F04A16",
	                     "GR1601101250000000012300695"));
	addSpecification(new Specification(
	                     "GT", 28, "A04A20",
	                     "GT82TRAJ01020000001210029690"));
	addSpecification(new Specification(
	                     "HR", 21, "F07F10",
	                     "HR1210010051863000160"));
	addSpecification(new Specification(
	                     "HU", 28, "F03F04F01F15F01",
	                     "HU42117730161111101800000000"));
	addSpecification(new Specification(
	                     "IE", 22, "U04F06F08",
	                     "IE29AIBK93115212345678"));
	addSpecification(new Specification(
	                     "IL", 23, "F03F03F13",
	                     "IL620108000000099999999"));
	addSpecification(new Specification(
	                     "IS", 26, "F04F02F06F10",
	                     "IS140159260076545510730339"));
	addSpecification(new Specification(
	                     "IT", 27, "U01F05F05A12",
	                     "IT60X0542811101000000123456"));
	addSpecification(new Specification(
	                     "KW", 30, "U04A22",
	                     "KW81CBKU0000000000001234560101"));
	addSpecification(new Specification(
	                     "KZ", 20, "F03A13",
	                     "KZ86125KZT5004100100"));
	addSpecification(new Specification(
	                     "LB", 28, "F04A20",
	                     "LB62099900000001001901229114"));
	addSpecification(new Specification(
	                     "LC", 32, "U04F24",
	                     "LC07HEMM000100010012001200013015"));
	addSpecification(new Specification(
	                     "LI", 21, "F05A12",
	                     "LI21088100002324013AA"));
	addSpecification(new Specification(
	                     "LT", 20, "F05F11",
	                     "LT121000011101001000"));
	addSpecification(new Specification(
	                     "LU", 20, "F03A13",
	                     "LU280019400644750000"));
	addSpecification(new Specification(
	                     "LV", 21, "U04A13",
	                     "LV80BANK0000435195001"));
	addSpecification(new Specification(
	                     "MC", 27, "F05F05A11F02",
	                     "MC5811222000010123456789030"));
	addSpecification(new Specification(
	                     "MD", 24, "U02A18",
	                     "MD24AG000225100013104168"));
	addSpecification(new Specification(
	                     "ME", 22, "F03F13F02",
	                     "ME25505000012345678951"));
	addSpecification(new Specification(
	                     "MK", 19, "F03A10F02",
	                     "MK07250120000058984"));
	addSpecification(new Specification(
	                     "MR", 27, "F05F05F11F02",
	                     "MR1300020001010000123456753"));
	addSpecification(new Specification(
	                     "MT", 31, "U04F05A18",
	                     "MT84MALT011000012345MTLCAST001S"));
	addSpecification(new Specification(
	                     "MU", 30, "U04F02F02F12F03U03",
	                     "MU17BOMM0101101030300200000MUR"));
	addSpecification(new Specification(
	                     "NL", 18, "U04F10",
	                     "NL91ABNA0417164300"));
	addSpecification(new Specification(
	                     "NO", 15, "F04F06F01",
	                     "NO9386011117947"));
	addSpecification(new Specification(
	                     "PK", 24, "U04A16",
	                     "PK36SCBL0000001123456702"));
	addSpecification(new Specification(
	                     "PL", 28, "F08F16",
	                     "PL61109010140000071219812874"));
	addSpecification(new Specification(
	                     "PS", 29, "U04A21",
	                     "PS92PALS000000000400123456702"));
	addSpecification(new Specification(
	                     "PT", 25, "F04F04F11F02",
	                     "PT50000201231234567890154"));
	addSpecification(new Specification(
	                     "RO", 24, "U04A16",
	                     "RO49AAAA1B31007593840000"));
	addSpecification(new Specification(
	                     "RS", 22, "F03F13F02",
	                     "RS35260005601001611379"));
	addSpecification(new Specification(
	                     "SA", 24, "F02A18",
	                     "SA0380000000608010167519"));
	addSpecification(new Specification(
	                     "SE", 24, "F03F16F01",
	                     "SE4550000000058398257466"));
	addSpecification(new Specification(
	                     "SI", 19, "F05F08F02",
	                     "SI56263300012039086"));
	addSpecification(new Specification(
	                     "SK", 24, "F04F06F10",
	                     "SK3112000000198742637541"));
	addSpecification(new Specification(
	                     "SM", 27, "U01F05F05A12",
	                     "SM86U0322509800000000270100"));
	addSpecification(new Specification(
	                     "ST", 25, "F08F11F02",
	                     "ST68000100010051845310112"));
	addSpecification(new Specification(
	                     "TL", 23, "F03F14F02",
	                     "TL380080012345678910157"));
	addSpecification(new Specification(
	                     "TN", 24, "F02F03F13F02",
	                     "TN5910006035183598478831"));
	addSpecification(new Specification(
	                     "TR", 26, "F05F01A16",
	                     "TR330006100519786457841326"));
	addSpecification(new Specification(
	                     "VG", 24, "U04F16",
	                     "VG96VPVG0000012345678901"));
	addSpecification(new Specification(
	                     "XK", 20, "F04F10F02",
	                     "XK051212012345678906"));
	addSpecification(new Specification(
	                     "AO", 25, "F21",
	                     "AO69123456789012345678901"));
	addSpecification(new Specification(
	                     "BF", 27, "F23",
	                     "BF2312345678901234567890123"));
	addSpecification(new Specification(
	                     "BI", 16, "F12",
	                     "BI41123456789012"));
	addSpecification(new Specification(
	                     "BJ", 28, "F24",
	                     "BJ39123456789012345678901234"));
	addSpecification(new Specification(
	                     "CI", 28, "U01F23",
	                     "CI17A12345678901234567890123"));
	addSpecification(new Specification(
	                     "CM", 27, "F23",
	                     "CM9012345678901234567890123"));
	addSpecification(new Specification(
	                     "CV", 25, "F21",
	                     "CV30123456789012345678901"));
	addSpecification(new Specification(
	                     "DZ", 24, "F20",
	                     "DZ8612345678901234567890"));
	addSpecification(new Specification(
	                     "IR", 26, "F22",
	                     "IR861234568790123456789012"));
	addSpecification(new Specification(
	                     "JO", 30, "A04F22",
	                     "JO15AAAA1234567890123456789012"));
	addSpecification(new Specification(
	                     "MG", 27, "F23",
	                     "MG1812345678901234567890123"));
	addSpecification(new Specification(
	                     "ML", 28, "U01F23",
	                     "ML15A12345678901234567890123"));
	addSpecification(new Specification(
	                     "MZ", 25, "F21",
	                     "MZ25123456789012345678901"));
	addSpecification(new Specification(
	                     "QA", 29, "U04A21",
	                     "QA30AAAA123456789012345678901"));
	addSpecification(new Specification(
	                     "SN", 28, "U01F23",
	                     "SN52A12345678901234567890123"));
	addSpecification(new Specification(
	                     "UA", 29, "F25",
	                     "UA511234567890123456789012345"));

}

namespace {
bool account_validate(text *iban) {
	char *ciban;

	Validate val;
	ciban = text_to_cstring(iban);

	return val.isValid(std::string(ciban));
}
}

extern "C"
{
	PG_FUNCTION_INFO_V1(iban_validate);

	Datum
	iban_validate(PG_FUNCTION_ARGS) {
		text     *iban = PG_GETARG_TEXT_P(0);

		bool r = account_validate(iban);

		PG_RETURN_BOOL(r);
	}
}
