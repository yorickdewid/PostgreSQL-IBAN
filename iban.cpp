/*
 * IBAN: PostgreSQL functions and datatype
 * Copyright © 2016 Yorick de Wid <yorick17@outlook.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *********************************************************************/

extern "C"
{

#include "postgres.h"

#include "utils/builtins.h"
#include "libpq/pqformat.h"

}

#include <regex>

class Specification {
  public:
	Specification(std::string structure, size_t length)
		: structure{ structure }
		, m_length{ length }
	{
	};
	
	inline size_t getLength() const noexcept
	{
		return m_length;
	}

	std::string structure;

private:
	size_t m_length;
};

class Validate {
  public:
	Validate();

	bool isValid(std::string arg);
	
	void addSpecification(std::string countryCode, size_t length, std::string structure) noexcept
	{
		specifications.emplace(countryCode, std::make_unique<Specification>(structure, length));
	}

	std::map<std::string, std::unique_ptr<Specification>> specifications;
};

/* Validator global instance. */
static Validate validator;

/**
* Calculates the MOD 97 10 of the passed IBAN as specified in ISO7064.
*
* @param string iban
* @returns {bool}
*/
static bool iso7064Mod97_10(std::string iban) {
	std::rotate(iban.begin(), iban.begin() + 4, iban.end());
	
	/* Will contain the letter substitutions */
	std::string numberstring;
	numberstring.reserve(iban.size());

	for (const auto& c : iban) {
		if (std::isdigit(c)) {
			numberstring += c;
		}
		if (std::isupper(c)) {
			numberstring += std::to_string(static_cast<int>(c) - 55);
		}
	}

	/*
	 * Implements a stepwise check for mod 97 in chunks of 9 at the first time
	 * then in chunks of seven prepended by the last mod 97 operation converted
	 * to a string
	 */
	size_t segstart = 0;
	int step = 9;
	std::string prepended;
	long number = 0;
	while (segstart < numberstring.length() - step) {
		number = std::stol(prepended + numberstring.substr(segstart, step));
		int remainder = number % 97;
		prepended = std::to_string(remainder);
		if (remainder < 10) {
			prepended = "0" + prepended;
		}
		segstart = segstart + step;
		step = 7;
	}

	number = std::stol(prepended + numberstring.substr(segstart));
	return number % 97 == 1;
}

/**
* Parse the IBAN structure used to configure each iban specification and returns a matching regular expression.
* a structure is composed of blocks of 3 characters (one letter and 2 digits). each block represents
* a logical group in the typical representation of the bban. for each group, the letter indicates which characters
* are allowed in this group and the following 2-digits number tells the length of the group.
*
* @param {string} structure the structure to parse
* @returns {regexp}
*/
static std::regex parseStructure(std::string structure) {
	std::smatch match;

	std::string::const_iterator text_iter = structure.cbegin();
	std::ostringstream result;

	while (std::regex_search(text_iter, structure.cend(), match, std::regex("(.{3})"))) {
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

bool Validate::isValid(std::string account) {
	/* Convert uppercase */
	std::transform(account.begin(), account.end(), account.begin(), toupper);

	/* Reject anything too small */
	if (account.length() < 3) {
		return false;
	}

	/* Match on country */
	const std::string& countryCode = account.substr(0, 2);
	const std::string& shortened = account.substr(4);

	const std::unique_ptr<Specification>& specFound = specifications[countryCode];
	if (!specFound) {
		return false;
	}

	/* Test accountnumber */
	return specFound->getLength() == account.length()
	              && std::regex_match(shortened, parseStructure(specFound->structure))
	              && iso7064Mod97_10(account);
}

Validate::Validate() {
	addSpecification(
	                     "AD", 24, "F04F04A12");
	addSpecification(
	                     "AE", 23, "F03F16");
	addSpecification(
	                     "AL", 28, "F08A16");
	addSpecification(
	                     "AT", 20, "F05F11");
	addSpecification(
	                     "AZ", 28, "U04A20");
	addSpecification(
	                     "BA", 20, "F03F03F08F02");
	addSpecification(
	                     "BE", 16, "F03F07F02");
	addSpecification(
	                     "BG", 22, "U04F04F02A08");
	addSpecification(
	                     "BH", 22, "U04A14");
	addSpecification(
	                     "BR", 29, "F08F05F10U01A01");
	addSpecification(
	                     "CH", 21, "F05A12");
	addSpecification(
	                     "CR", 21, "F03F14");
	addSpecification(
	                     "CY", 28, "F03F05A16");
	addSpecification(
	                     "CZ", 24, "F04F06F10");
	addSpecification(
	                     "DE", 22, "F08F10");
	addSpecification(
	                     "DK", 18, "F04F09F01");
	addSpecification(
	                     "DO", 28, "U04F20");
	addSpecification(
	                     "EE", 20, "F02F02F11F01");
	addSpecification(
	                     "ES", 24, "F04F04F01F01F10");
	addSpecification(
	                     "FI", 18, "F06F07F01");
	addSpecification(
	                     "FO", 18, "F04F09F01");
	addSpecification(
	                     "FR", 27, "F05F05A11F02");
	addSpecification(
	                     "GB", 22, "U04F06F08");
	addSpecification(
	                     "GE", 22, "U02F16");
	addSpecification(
	                     "GI", 23, "U04A15");
	addSpecification(
	                     "GL", 18, "F04F09F01");
	addSpecification(
	                     "GR", 27, "F03F04A16");
	addSpecification(
	                     "GT", 28, "A04A20");
	addSpecification(
	                     "HR", 21, "F07F10");
	addSpecification(
	                     "HU", 28, "F03F04F01F15F01");
	addSpecification(
	                     "IE", 22, "U04F06F08");
	addSpecification(
	                     "IL", 23, "F03F03F13");
	addSpecification(
	                     "IS", 26, "F04F02F06F10");
	addSpecification(
	                     "IT", 27, "U01F05F05A12");
	addSpecification(
	                     "KW", 30, "U04A22");
	addSpecification(
	                     "KZ", 20, "F03A13");
	addSpecification(
	                     "LB", 28, "F04A20");
	addSpecification(
	                     "LC", 32, "U04F24");
	addSpecification(
	                     "LI", 21, "F05A12");
	addSpecification(
	                     "LT", 20, "F05F11");
	addSpecification(
	                     "LU", 20, "F03A13");
	addSpecification(
	                     "LV", 21, "U04A13");
	addSpecification(
	                     "MC", 27, "F05F05A11F02");
	addSpecification(
	                     "MD", 24, "U02A18");
	addSpecification(
	                     "ME", 22, "F03F13F02");
	addSpecification(
	                     "MK", 19, "F03A10F02");
	addSpecification(
	                     "MR", 27, "F05F05F11F02");
	addSpecification(
	                     "MT", 31, "U04F05A18");
	addSpecification(
	                     "MU", 30, "U04F02F02F12F03U03");
	addSpecification(
	                     "NL", 18, "U04F10");
	addSpecification(
	                     "NO", 15, "F04F06F01");
	addSpecification(
	                     "PK", 24, "U04A16");
	addSpecification(
	                     "PL", 28, "F08F16");
	addSpecification(
	                     "PS", 29, "U04A21");
	addSpecification(
	                     "PT", 25, "F04F04F11F02");
	addSpecification(
	                     "RO", 24, "U04A16");
	addSpecification(
	                     "RS", 22, "F03F13F02");
	addSpecification(
	                     "SA", 24, "F02A18");
	addSpecification(
	                     "SE", 24, "F03F16F01");
	addSpecification(
	                     "SI", 19, "F05F08F02");
	addSpecification(
	                     "SK", 24, "F04F06F10");
	addSpecification(
	                     "SM", 27, "U01F05F05A12");
	addSpecification(
	                     "ST", 25, "F08F11F02");
	addSpecification(
	                     "TL", 23, "F03F14F02");
	addSpecification(
	                     "TN", 24, "F02F03F13F02");
	addSpecification(
	                     "TR", 26, "F05F01A16");
	addSpecification(
	                     "VG", 24, "U04F16");
	addSpecification(
	                     "XK", 20, "F04F10F02");
	addSpecification(
	                     "AO", 25, "F21");
	addSpecification(
	                     "BF", 27, "F23");
	addSpecification(
	                     "BI", 16, "F12");
	addSpecification(
	                     "BJ", 28, "F24");
	addSpecification(
	                     "CI", 28, "U01F23");
	addSpecification(
	                     "CM", 27, "F23");
	addSpecification(
	                     "CV", 25, "F21");
	addSpecification(
	                     "DZ", 24, "F20");
	addSpecification(
	                     "IR", 26, "F22");
	addSpecification(
	                     "JO", 30, "A04F22");
	addSpecification(
	                     "MG", 27, "F23");
	addSpecification(
	                     "ML", 28, "U01F23");
	addSpecification(
	                     "MZ", 25, "F21");
	addSpecification(
	                     "QA", 29, "U04A21");
	addSpecification(
	                     "SN", 28, "U01F23");
	addSpecification(
	                     "UA", 29, "F25");
	addSpecification(
	                     "EG", 27, "F23");
	addSpecification(
	                     "CG", 27, "F23");
	addSpecification(
	                     "GA", 27, "F23");
}

/**
* Separate CXX and C logic to minimize unexpected or malformed symbols due to
* language conversions. Also demark all exceptions the std++ can throw since
* PostgreSQL is not able to handle them.
*
* @param {string} iban
* @returns {bool}
*/
namespace {

bool account_validate_text(text *iban) {
	char *ciban;
	bool result;

	ciban = text_to_cstring(iban);

	try {
		result = validator.isValid(ciban);
	} catch (std::exception& e) {
		elog(ERROR, "%s", e.what());
		return false;
	}

	return result;
}

bool account_validate_str(char *iban) {
	bool result;

	try {
		result = validator.isValid(iban);
	} catch (std::exception& e) {
		elog(ERROR, "%s", e.what());
		return false;
	}

	return result;
}

} // namespace

extern "C"
{

PG_MODULE_MAGIC;

typedef char Iban;

/**************************************************************************
 * Input/Output functions
 **************************************************************************/

PG_FUNCTION_INFO_V1(ibanin);

Datum
ibanin(PG_FUNCTION_ARGS) {
	char	   *inputText = PG_GETARG_CSTRING(0);

	if (!account_validate_str(inputText))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
					errmsg("invalid iban format for value: \"%s\"",
						inputText)));

	PG_RETURN_TEXT_P(cstring_to_text(inputText));
}

/* Convert type output */

PG_FUNCTION_INFO_V1(ibanout);

Datum
ibanout(PG_FUNCTION_ARGS) {
	Iban       *iban = (Iban *) PG_GETARG_DATUM(0);

	PG_RETURN_CSTRING(TextDatumGetCString(iban));
}

/**************************************************************************
 * Binary Input/Output functions
 **************************************************************************/

PG_FUNCTION_INFO_V1(ibanrecv);

Datum
ibanrecv(PG_FUNCTION_ARGS) {
	StringInfo     buf = (StringInfo) PG_GETARG_POINTER(0);
	text           *result;
	Iban           *str;
	int            nbytes;

	str = pq_getmsgtext(buf, buf->len - buf->cursor, &nbytes);

	result = cstring_to_text_with_len(str, nbytes);
	pfree(str);
	PG_RETURN_TEXT_P(result);
}

PG_FUNCTION_INFO_V1(ibansend);

Datum
ibansend(PG_FUNCTION_ARGS) {
	text       *t = PG_GETARG_TEXT_PP(0);
	StringInfoData buf;

	pq_begintypsend(&buf);
	pq_sendtext(&buf, VARDATA_ANY(t), VARSIZE_ANY_EXHDR(t));
	PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

/* Manually verify a text */

PG_FUNCTION_INFO_V1(iban_validate);

Datum
iban_validate(PG_FUNCTION_ARGS) {
	text     *iban = PG_GETARG_TEXT_P(0);

	bool result = account_validate_text(iban);

	PG_RETURN_BOOL(result);
}

} // extern "C"
