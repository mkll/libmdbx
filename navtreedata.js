/*
@licstart  The following is the entire license notice for the
JavaScript code in this file.

Copyright (C) 1997-2019 by Dimitri van Heesch

This program is free software; you can redistribute it and/or modify
it under the terms of version 2 of the GNU General Public License as published by
the Free Software Foundation

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

@licend  The above is the entire license notice
for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "libmdbx", "index.html", [
    [ "Overall", "index.html", [
      [ "Brief", "index.html#brief", null ],
      [ "Table of Contents", "index.html#toc", null ],
      [ "Mithril DB", "index.html#mithril", null ],
      [ "History", "index.html#autotoc_md1", [
        [ "Acknowledgments", "index.html#autotoc_md2", null ]
      ] ],
      [ "Contributors", "index.html#autotoc_md3", null ],
      [ "License", "index.html#autotoc_md4", null ]
    ] ],
    [ "Introduction", "intro.html", [
      [ "Characteristics", "intro.html#characteristics", [
        [ "Preface", "intro.html#preface", null ],
        [ "Features", "intro.html#autotoc_md5", null ],
        [ "Limitations", "intro.html#autotoc_md6", null ],
        [ "Gotchas", "intro.html#autotoc_md7", null ],
        [ "Comparison with other databases", "intro.html#autotoc_md8", null ]
      ] ],
      [ "Improvements beyond LMDB", "intro.html#autotoc_md9", [
        [ "Added Features", "intro.html#autotoc_md10", null ],
        [ "Added Abilities", "intro.html#autotoc_md11", null ],
        [ "Other fixes and specifics", "intro.html#autotoc_md12", null ]
      ] ],
      [ "Restrictions & Caveats", "intro.html#restrictions", [
        [ "Troubleshooting the LCK-file", "intro.html#autotoc_md13", null ],
        [ "Remote filesystems", "intro.html#autotoc_md14", null ],
        [ "Child processes", "intro.html#autotoc_md15", null ],
        [ "Read-only mode", "intro.html#autotoc_md16", null ],
        [ "One thread - One transaction", "intro.html#autotoc_md17", null ],
        [ "Do not open twice", "intro.html#autotoc_md18", null ],
        [ "Long-lived read transactions", "intro.html#autotoc_md19", null ],
        [ "Space reservation", "intro.html#autotoc_md20", null ]
      ] ],
      [ "Performance comparison", "intro.html#performance", [
        [ "Integral performance", "intro.html#autotoc_md21", null ],
        [ "Read Scalability", "intro.html#autotoc_md23", null ],
        [ "Sync-write mode", "intro.html#autotoc_md25", null ],
        [ "Lazy-write mode", "intro.html#autotoc_md27", null ],
        [ "Async-write mode", "intro.html#autotoc_md29", null ],
        [ "Cost comparison", "intro.html#autotoc_md31", null ]
      ] ]
    ] ],
    [ "Usage", "usage.html", [
      [ "Getting the libmdbx", "usage.html#getting", [
        [ "Source code embedding", "usage.html#autotoc_md32", null ],
        [ "Building", "usage.html#autotoc_md33", [
          [ "Linux and other platforms with GNU Make", "usage.html#autotoc_md35", null ],
          [ "FreeBSD and related platforms", "usage.html#autotoc_md36", null ],
          [ "Windows", "usage.html#autotoc_md37", null ],
          [ "Windows Subsystem for Linux", "usage.html#autotoc_md38", null ],
          [ "MacOS", "usage.html#autotoc_md39", null ],
          [ "Android", "usage.html#autotoc_md40", null ],
          [ "iOS", "usage.html#autotoc_md41", null ]
        ] ]
      ] ],
      [ "Getting started", "usage.html#starting", [
        [ "Cursors", "usage.html#Cursors", null ],
        [ "Summarizing the opening", "usage.html#autotoc_md42", null ],
        [ "Threads and processes", "usage.html#autotoc_md43", null ],
        [ "Transactions, rollbacks etc", "usage.html#autotoc_md44", null ],
        [ "Duplicate keys aka Multi-values", "usage.html#autotoc_md45", null ],
        [ "Some optimization", "usage.html#autotoc_md46", null ],
        [ "Cleaning up", "usage.html#autotoc_md47", null ],
        [ "Now read up on the full API!", "usage.html#autotoc_md48", null ]
      ] ],
      [ "Bindings", "usage.html#bindings", null ]
    ] ],
    [ "ChangeLog", "md__change_log.html", [
      [ "v0.9.x (in the development):", "md__change_log.html#autotoc_md52", null ],
      [ "v0.8.2 2020-07-06:", "md__change_log.html#autotoc_md53", null ],
      [ "v0.8.1 2020-06-12:", "md__change_log.html#autotoc_md54", null ],
      [ "v0.8.0 2020-06-05:", "md__change_log.html#autotoc_md55", null ],
      [ "v0.7.0 2020-03-18:", "md__change_log.html#autotoc_md56", null ],
      [ "v0.6.0 2020-01-21:", "md__change_log.html#autotoc_md57", null ],
      [ "v0.5.0 2019-12-31:", "md__change_log.html#autotoc_md58", null ],
      [ "v0.4.0 2019-12-02:", "md__change_log.html#autotoc_md59", null ]
    ] ],
    [ "Deprecated List", "deprecated.html", null ],
    [ "Modules", "modules.html", "modules" ],
    [ "Data Structures", "annotated.html", [
      [ "Data Structures", "annotated.html", "annotated_dup" ],
      [ "Data Structure Index", "classes.html", null ],
      [ "Data Fields", "functions.html", [
        [ "All", "functions.html", null ],
        [ "Variables", "functions_vars.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "Globals", "globals.html", [
        [ "All", "globals.html", "globals_dup" ],
        [ "Functions", "globals_func.html", null ],
        [ "Variables", "globals_vars.html", null ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Enumerations", "globals_enum.html", null ],
        [ "Enumerator", "globals_eval.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"group__c__cursors.html#ga501baff3bcff7e8e7818b4d412a2a6d5",
"group__c__extra.html#ga7ce13e608f1ff54b6cafb5497b081adf",
"struct_m_d_b_x__envinfo.html#a2288193669b48107ead4ef049d999c3f"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';