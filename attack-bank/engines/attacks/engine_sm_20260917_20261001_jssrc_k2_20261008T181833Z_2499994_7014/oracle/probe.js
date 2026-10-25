const re = new RegExp("\\xC0\\xD0" + "(a?)".repeat(40), "y");
re.exec("\xC0\xD0");
