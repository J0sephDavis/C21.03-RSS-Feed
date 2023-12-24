/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#ifndef c1402_regex_header
#define c1402_regex_header
#include <cctype>
#include <stdexcept>
#include <string.h>
#include <iostream>
#include <string>
/* 1."abc" -> 	acceptsthe literal string "abc"
 * 2."a?bc" ->	accepts "abc" or "bc".
 * 3."ab*c" -> 	accepts "ac", "abc", "abbbbbbbbbbbbbbbbc"
 * 4."ab+c" -> 	accepts "abc", "abbbbbbbbbbbbbbc"
 * 5."ab(CD)" -> 	accepts "abC", "abD"
 * 6."ab(C*D*)"->	accepts "ab", "abC", "abD", "abCCCCCCCC", "abDDDDDD"
 * 7."ab(CD)*" -> acceptance to be determined when compilation/parsing code is rewritten. (Possibly make equivalent to input string 6? Pre-processing or somesuch method)
 * 8. "ab." -> 	accepts "abZ", "abf", "ab9"
 * 9. "ab&" -> accepts "aba", "abB", "abz"
 * 10."ab#" -> accepts "ab1", "ab2", "ab9"
 * 11. "ab.*" -> accepts "ab", "ab1", "ab22222222222", "abf", "abZZZZZZZZz"
 * */
namespace C1402_regex {
enum rules {
	R_DEFAULT,
	R_STAR,
	R_PLUS,
	R_OPT,
};
enum substitution_type {
	S_LITERAL,
	S_DIGIT, 	//DIGIT - numbers 0-9
	S_ALPHA, 	//ALPHA - letters a-zA-Z
	S_ALNUM, //ALPHANUMERIC - any letter or number
};
//
//
class regex {
	public:
		regex(int, substitution_type);
		virtual ~regex() {
			if (alternate != NULL) {
				delete alternate;
			}
			//because the alternate will also point to next
			else if (next != NULL) {
				delete next;
			}
		}
		int getLiteral();

		regex* getNext();
		virtual void setNext(regex*);
		//return a char* to the pointer where the match ends. This can be used to determine where the match occurred, and its length
		virtual char* match_here(char* text);
		virtual rules getRule() {
			return R_DEFAULT;
		}
		bool accepts(int character);
		void addAlternate(regex*);
		regex* getAlternate();
	protected:
		class regex* next;
		class regex* alternate;
		substitution_type sub_rule;
	private:
		//literal's value will not matter when substitute != R_CHAR
		int literal;
};
class regex_star : public regex {
	public:
		regex_star(int _literal, substitution_type _sub_rule = S_LITERAL) :
			regex(_literal,_sub_rule) {};
		char* match_here(char* text) override;
		rules getRule() override { return R_STAR; };
};
class regex_plus : public regex {
	public:
		regex_plus(int _literal, substitution_type _sub_rule = S_LITERAL) :
			regex(_literal, _sub_rule) {};
		char* match_here(char* text) override;
		rules getRule() override { return R_PLUS; };
};
class regex_opt : public regex {
	public:
		regex_opt(int _literal, substitution_type _sub_rule = S_LITERAL) :
			regex(_literal, _sub_rule) {};
		char* match_here(char* text) override;
		rules getRule() override { return R_OPT; };
};
namespace utils {
	rules symbol_to_rrule(char);
	substitution_type symbol_to_srule(char);
	regex* create_from_string(std::string);
	static std::string rule_to_string(rules);
}//end utils namespace
}//end regex namespace
//
//std::string utils::sub_to_string(substitution_type sub_rule) {
//	switch (sub_rule) {
//		case(S_ALNUM):
//			return "ALNUM";
//		case(S_ALPHA):
//			return "ALPHA";
//		case(S_DIGIT):
//			return "DIGIT";
//		default:
//			return "ERROR";
//		case(S_LITERAL):
//			return "LITERAL";		
//	}
//};
#endif
