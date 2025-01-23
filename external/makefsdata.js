const fs = require("fs");
const path = require("path");
const readline = require('readline');

// Directory for markup files
const markupDir = path.join(__dirname, "../", "markup");

// Open the main fsdata.c and fsdata.h files
const commonHFile = fs.createWriteStream(path.join(__dirname, "../", "include", "fsdata.h"));

commonHFile.write(`#ifndef FSDATA_H\n#define FSDATA_H`);

// Process each file in the markup directory
const markupFiles = fs.readdirSync(markupDir).filter((file) => !file.startsWith("."));

markupFiles.forEach((file, index) => {
    const filePath = path.join(markupDir, file);
    const fileContent = fs.readFileSync(filePath, 'utf-8');

    // Create a valid variable name for the file
    const varName = file.replace(/[^\w]/g, "_");

    commonHFile.write(`\nstatic const char ${varName}_markup[] = `);

    const fileLines = fileContent.split('\n')

    // Write the file data as a byte array
    for (let i = 0; i < fileLines.length; i++) {
        console.log(fileLines[i])
        commonHFile.write(`\n"${fileLines[i].replace(/["]/g, '\\$&')}\\n"`);
        
    }
    commonHFile.write(';\n\n');

});
commonHFile.write('#endif // FSDATA_H')
commonHFile.end();

console.log("File system data generation complete.");
