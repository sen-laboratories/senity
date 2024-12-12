= Integrating MD4C Markdown Parser

* parsing into a backup structure that holds offsets and parser info as key/value map in a BMessage
* needed because markup and text offsets overlap, e.g. H1 starts with same offset as header text,
  or block ends together with para

== Message structure (abandoned)

* what code is only symbolic and not functional, just used for debugging
* key = text offset from parsing, 0 - TextLength
* value = Message with:
  * key = MD_CLASS
  * value = Message with:
    * key = MD_BLOCK_TYPE or MD_SPAN_TYPE or MD_TEXT_TYPE, depending on MD_CLASS
    * value = MD_X_DETAIL message
    
== Example

* typical Readme with headers (blocks), para sections and list sections with link spans (like this one):

0 => MSG
	MD_BLOCK => MSG
		HEADER => MSG
			level = 1
	MD_TEXT => MSG {}
20 => MSG
	MD_BLOCK[0] => MSG
		LIST => MSG
			type = UL
	MD_BLOCK[1] => MSG
		ITEM => MSG
			...
	MD_TEXT	=> MSG {}
	
* abandoned because:
  * Haiku specific, not portable
  * cumbersome for iteration -> we need a stack anyway
  * no quick way to search "nearest" key for a given offset
  
== Offset Map with markup stack as values

* simple, fast, intuitive and portable standard C++
* we keep a map for the offsets (keys are reused for nested blocks/spans/text as they are kept in a stack stored under the offset key)
* with std::map::lower_bound and std::map::upper_bound we can search for the nearest markup info at a given index
* we can then simply iterate over the returned stack for styling.
