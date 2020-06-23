macro(set_doxygen_vars)
  set(DOXYGEN_PROJECT_NAME          Molassembler)
  set(DOXYGEN_PROJECT_NUMBER        ${molassembler_VERSION})
  set(DOXYGEN_PROJECT_BRIEF         "Molecule graph and conformer library")
  set(DOXYGEN_STRIP_FROM_PATH       ${molassembler_SOURCE_DIR}/src/)
# Note this has requires multiple escapes for backslashes and semicolons (that's cmake's list character otherwise)
  set(DOXYGEN_ALIASES
    "diffminus{1}=\\htmlonly<span style='color:red\;text-decoration:line-through'>\\1</span>\\endhtmlonly\" \\\n\"diffplus{1}=\\htmlonly<span style='color:green\;'>\\1</span>\\endhtmlonly\" \\\n\"masm{1}=\\link Scine::Molassembler::\\1 \\1\\endlink\" \\\n\"masm{2}=\\link Scine::Molassembler::\\1 \\2\\endlink\" \\\n\"complexity{1}=\\b Complexity \\1\" \\\n\"math{1}=\\f$\\1\\f$"
  )
  set(DOXYGEN_OPTIMIZE_OUTPUT_FOR_C YES)
  set(DOXYGEN_TOC_INCLUDE_HEADINGS  0)
  set(DOXYGEN_BUILTIN_STL_SUPPORT   YES)
  set(DOXYGEN_EXTRACT_PRIVATE       YES)
  set(DOXYGEN_EXTRACT_PRIV_VIRTUAL  YES)
  set(DOXYGEN_GENERATE_TODOLIST     NO)
  set(DOXYGEN_GENERATE_TESTLIST     NO)
  set(DOXYGEN_GENERATE_BUGLIST      NO)
  set(DOXYGEN_GENERATE_DEPRECATEDLIST= NO)
  set(DOXYGEN_LAYOUT_FILE           ${molassembler_SOURCE_DIR}/doc/layout.xml)
  set(DOXYGEN_CITE_BIB_FILES        ${molassembler_SOURCE_DIR}/doc/library.bib)
  set(DOXYGEN_QUIET                 YES)
  set(DOXYGEN_WARN_IF_UNDOCUMENTED  NO)
  set(DOXYGEN_WARN_LOGFILE          doxygen.errlog)
  set(DOXYGEN_INPUT
    "${molassembler_SOURCE_DIR}/src/Molassembler/"
    "${molassembler_SOURCE_DIR}/doc/manual.dox"
    "${molassembler_SOURCE_DIR}/doc/known-issues.dox"
    "${molassembler_SOURCE_DIR}/doc/dev-details.dox"
  )
  set(DOXYGEN_FILE_PATTERNS         "*.h;*.hxx;*.hpp;*.dox")
  set(DOXYGEN_RECURSIVE             YES)
  set(DOXYGEN_EXCLUDE_PATTERNS      */test/*)
  set(DOXYGEN_EXCLUDE_SYMBOLS       Detail)
  set(DOXYGEN_IMAGE_PATH            ${molassembler_SOURCE_DIR}/doc/image/)
# set(DOXYGEN_HTML_OUTPUT           doc)
  set(DOXYGEN_HTML_HEADER           ${molassembler_SOURCE_DIR}/doc/header.html)
  set(DOXYGEN_HTML_FOOTER           ${molassembler_SOURCE_DIR}/doc/footer.html)
  set(DOXYGEN_HTML_STYLESHEET       ${molassembler_SOURCE_DIR}/doc/stylesheet.css)
  set(DOXYGEN_HTML_COLORSTYLE_HUE   207)
  set(DOXYGEN_HTML_COLORSTYLE_SAT   112)
  set(DOXYGEN_DISABLE_INDEX         YES)
  set(DOXYGEN_GENERATE_TREEVIEW     YES)
  set(DOXYGEN_FORMULA_MACROFILE     ${molassembler_SOURCE_DIR}/doc/macrofile.tex)
  set(DOXYGEN_USE_MATHJAX           YES)
  set(DOXYGEN_GENERATE_LATEX        NO)
  set(DOXYGEN_LATEX_CMD_NAME        latex)
  set(DOXYGEN_MACRO_EXPANSION       YES)
  set(DOXYGEN_SEARCH_INCLUDES       NO)
  set(DOXYGEN_PREDEFINED
    "MASM_EXPORT="
    "MASM_NO_EXPORT="
  )
  set(DOXYGEN_COLLABORATION_GRAPH   NO)
  set(DOXYGEN_GROUP_GRAPHS          NO)
  set(DOXYGEN_DOT_IMAGE_FORMAT      svg)
  set(DOXYGEN_DOTFILE_DIRS          ${molassembler_SOURCE_DIR}/doc/graphs/)
endmacro()

macro(unset_doxygen_vars)
  unset(DOXYGEN_PROJECT_NAME)
  unset(DOXYGEN_PROJECT_NUMBER)
  unset(DOXYGEN_PROJECT_BRIEF)
  unset(DOXYGEN_STRIP_FROM_PATH)
  unset(DOXYGEN_ALIASES)
  unset(DOXYGEN_OPTIMIZE_OUTPUT_FOR_C)
  unset(DOXYGEN_TOC_INCLUDE_HEADINGS)
  unset(DOXYGEN_BUILTIN_STL_SUPPORT)
  unset(DOXYGEN_EXTRACT_PRIVATE)
  unset(DOXYGEN_EXTRACT_PRIV_VIRTUAL)
  unset(DOXYGEN_GENERATE_TODOLIST)
  unset(DOXYGEN_GENERATE_TESTLIST)
  unset(DOXYGEN_GENERATE_BUGLIST)
  unset(DOXYGEN_GENERATE_DEPRECATEDLIST=)
  unset(DOXYGEN_LAYOUT_FILE)
  unset(DOXYGEN_CITE_BIB_FILES)
  unset(DOXYGEN_QUIET)
  unset(DOXYGEN_WARN_IF_UNDOCUMENTED)
  unset(DOXYGEN_WARN_LOGFILE)
  unset(DOXYGEN_INPUT)
  unset(DOXYGEN_FILE_PATTERNS)
  unset(DOXYGEN_RECURSIVE)
  unset(DOXYGEN_EXCLUDE_PATTERNS)
  unset(DOXYGEN_EXCLUDE_SYMBOLS)
  unset(DOXYGEN_IMAGE_PATH)
  unset(DOXYGEN_HTML_HEADER)
  unset(DOXYGEN_HTML_FOOTER)
  unset(DOXYGEN_HTML_STYLESHEET)
  unset(DOXYGEN_HTML_COLORSTYLE_HUE)
  unset(DOXYGEN_HTML_COLORSTYLE_SAT)
  unset(DOXYGEN_DISABLE_INDEX)
  unset(DOXYGEN_GENERATE_TREEVIEW)
  unset(DOXYGEN_FORMULA_MACROFILE)
  unset(DOXYGEN_USE_MATHJAX)
  unset(DOXYGEN_GENERATE_LATEX)
  unset(DOXYGEN_LATEX_CMD_NAME)
  unset(DOXYGEN_MACRO_EXPANSION)
  unset(DOXYGEN_SEARCH_INCLUDES)
  unset(DOXYGEN_PREDEFINED)
  unset(DOXYGEN_COLLABORATION_GRAPH)
  unset(DOXYGEN_GROUP_GRAPHS)
  unset(DOXYGEN_DOT_IMAGE_FORMAT)
  unset(DOXYGEN_DOTFILE_DIRS)
endmacro()
