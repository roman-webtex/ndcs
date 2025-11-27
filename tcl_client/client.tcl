#!/bin/wish
encoding system utf-8

package require Tk
package require tablelist
package require apave

set tcl_precision 12
set ::fSize 8
set ::windows_prefix "S:"
set ::unix_prefix "/home/roman/share"
set ::host 192.168.1.10
set ::port 7474
set ::search_pattern "*"
set ::text_pattern ""
set ::find_in_zip 0

switch $::tcl_platform(platform) {     
    windows {         
        set ::filename_prefix $::windows_prefix     
        set ::explorer explorer
        set ::window_size "[winfo screenwidth .]x[expr {[winfo screenheight .] - 100}]+0+0"
    } 
    unix {         
        set ::filename_prefix $::unix_prefix     
        set ::explorer xdg-open
        set ::window_size "[winfo screenwidth .]x[winfo screenheight .]+0+0"
    } 
}

set ::search_dir ""

foreach font_name [font names] {
    font configure $font_name -size $::fSize
}
    
proc main {} {
    apave::initWM
    buildMainWindow
}

proc fif_client {host port} {
    set s [socket $host $port]
    fconfigure $s -buffering line
    return $s
}


proc buildMainWindow {} {
        apave::APave create ::pave

        set ::tblcols {0 {Назва файлу} left}
        
        toplevel .mainFrame
        wm title .mainFrame "search"
        wm attributes .mainFrame -fullscreen 1

        set content {
            {Menu - - - - - {-array {File "&Файл"  Sys "&Система"}} fillMenuAction}
            {fraSearch - - - - {pack -side top -fill x -expand 0 -padx 5 -pady 3} {-relief flat}}
            {fraSearch.labGetDir - - - - {pack -side left -padx 5 -pady 3} {-t "Виберіть каталог :" -font {-size $::fSize} }}
            {fraSearch.dirGetDir fraSearch.labGetDir L 1 1 {pack -fill x} {-tvar ::search_dir -title {Choose a directory} -initialdir $::filename_prefix}}
            {fraFile - - - - {pack -side top -fill x -expand 0 -padx 5 -pady 3} {-relief flat}}
            {fraFile.labGetFile - - - - {pack -side left -padx 5 -pady 3} {-t "Виберіть шаблон файлу :" -font {-size $::fSize} }}
            {fraFile.entGetFile fraFile.labGetFile L 1 1 {pack -fill x} {-tvar ::search_pattern}}
            {fraText - - - - {pack -side top -fill x -expand 0 -padx 5 -pady 3} {-relief flat}}
            {fraText.labGetText - - - - {pack -side left -padx 5 -pady 3} {-t "Шаблон пошуку :" -font {-size $::fSize} }}
            {fraText.entGetText fraText.labGetText L 1 1 {pack -fill x} {-tvar ::text_pattern}}
            {fraZip - - - - {pack -side top -fill x -expand 0 -padx 5 -pady 3} {-relief flat}}
            {fraZip.chbZip - - - - {pack -side left -padx 5 -pady 3 -fill x} {-t "Шукати в архівах :" -var ::find_in_zip -state disabled}}
            {fraPro - - - - {pack -side top -fill x -expand 0 -padx 5 -pady 3} {-relief flat}}
            {fraPro.proPercent - - - - {pack -side left -padx 5 -pady 3 -fill x -expand 1} {-mode indeterminate}}
            {fraTable - - - - {pack -side top -fill both -expand 1 -padx 5 -pady 3}}
            {fraTable.TblMain - - - - {pack -side left -fill both -expand 1} {-h 7 -lbxsel but -columns {$::tblcols}}}
            {fraTable.sbv fraTable.tblMain L - - {pack -side left -after %w}}
            {fraTable.sbh fraTable.tblMain T - - {pack -side left -before %w}}
            {fraButton - - - - {pack -side top -fill x -expand 0 -padx 5 -pady 3} {-relief flat}}
            {fraButton.butOk - - - - {pack -side right -padx 5 -pady 3} {-t "Розпочати" -com startSearch }}
            {fraButton.butCancel fraButton.butOk L 1 1 {pack -side right -padx 5 -pady 3 } {-t "Відмінити" -com exit}}
        }

        ::pave paveWindow .mainFrame $content

        set ::dbTable [::pave TblMain]


        $::dbTable columnconfigure 0 -name file_name -maxwidth 0 

        
        bind .mainFrame <F10> {exit}
        bind .mainFrame <Control-q> {exit}
        bind .mainFrame <Double-Button-1> {openFileAction}
        wm protocol .mainFrame WM_DELETE_WINDOW {prog_exit}
}

proc startSearch {} {
    if {$::find_in_zip == 0} {
        set in_zip "nozip"
    }

    $::dbTable delete 0 end
    update

    regsub $::filename_prefix $::search_dir "" s_dir
    set s [fif_client $::host $::port]
    puts $s "$s_dir<{$::search_pattern} {$::text_pattern}\0"

    while {[gets $s line] >= 0 } {
        if {[lindex [split $line ":"] 0] == "Всього "} {
            .mainFrame.fraPro.proPercent configure -maximum [lindex [split $line ":"] 1]
            $::dbTable insert end [list "$line. Знайдено:"]
        } elseif {[lindex [split $line ":"] 0] == ""} {
            .mainFrame.fraPro.proPercent step
            switch $::tcl_platform(platform) {     
                windows {         
                    .mainFrame.fraPro.proPercent configure -text [lindex [split $line ":"] 1]
                }
            }
        } else {
            $::dbTable insert end [list $line]
        }
        $::dbTable yview moveto 1.0
        update
    }
    .mainFrame.fraPro.proPercent configure -value 0
    update
}

proc openFileAction {} {
    set fnam [string map [list "\\" "[file separator]" "/" "[file separator]"] "[file nativename [lindex [lindex [$::dbTable get [$::dbTable curselection] [$::dbTable curselection]] 0] 0]]"]
    set fnam [file nativename $::search_dir[string trimright [string trimleft [lindex [split $fnam ">"] 0] "."]  " "]]
    catch { [exec $::explorer "$fnam"] errorMessage }
}

proc fillMenuAction {} {
    set m .mainFrame.menu.file
    $m add command -label "Вихід" -command {prog_exit}
        
    set m .mainFrame.menu.sys
    $m add command -label "Допомога по пошуку" -command {searchHelpAction}
    #$m add separator
    #$m add command -label "Налаштування" -command {changePassAction}
}


proc searchHelpAction {} {
    set helpText {
        .      - будь-якій один символ. 
        К*     - нуль або більше симолів К.
        К+     - один або більше символів К.
        К?     - нуль або один символ К.
        К|М    - або К або М.
        [asdg] - будь-якій символ зі списку.
        [^asd] - будь-якій символ НЕ зі списку.
        [a-z]  - диапазон від a до z.
        [0-9]  - диапазон від 0 до 9.

        [0-9]+ один - спочатку цифри, одна або більше, 
                      потім пробіл, потім 'один':
                      123 одиниці;
                      5 одиниць;
        одиниц.     -> одиниці, одиниця, одиниць
    }

    tk_messageBox -message "Пошук по шаблонах" -detail $helpText
    return
}

proc prog_exit {} {
    exit
}

main



