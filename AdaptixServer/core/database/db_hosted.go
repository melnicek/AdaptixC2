package database

import (
	"AdaptixServer/core/utils/logs"
	"database/sql"
	"errors"
	"fmt"
)

type HostedFileData struct {
	FileId     string `json:"h_file_id"`
	Slug       string `json:"h_slug"`
	FileName   string `json:"h_file_name"`
	LocalPath  string `json:"-"`
	FileSize   int64  `json:"h_file_size"`
	MimeType   string `json:"h_mime_type"`
	Source     string `json:"h_source"`
	SourceMeta string `json:"h_source_meta"`
	Uploader   string `json:"h_uploader"`
	Downloads  int64  `json:"h_downloads"`
	Date       int64  `json:"h_date"`
}

func (dbms *DBMS) DbHostedInsert(data HostedFileData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	insertQuery := `INSERT OR IGNORE INTO HostedFiles (FileId, Slug, FileName, LocalPath, FileSize, MimeType, Source, SourceMeta, Uploader, Downloads, Date) VALUES(?,?,?,?,?,?,?,?,?,?,?);`
	result, err := dbms.database.Exec(insertQuery, data.FileId, data.Slug, data.FileName, data.LocalPath, data.FileSize, data.MimeType, data.Source, data.SourceMeta, data.Uploader, data.Downloads, data.Date)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("hosted file %s already exists", data.FileId)
	}
	return nil
}

func (dbms *DBMS) DbHostedUpdateDownloads(fileId string, downloads int64) {
	if !dbms.DatabaseExists() {
		return
	}
	_, _ = dbms.database.Exec(`UPDATE HostedFiles SET Downloads = ? WHERE FileId = ?;`, downloads, fileId)
}

func (dbms *DBMS) DbHostedDelete(fileId string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	deleteQuery := `DELETE FROM HostedFiles WHERE FileId = ?;`
	result, err := dbms.database.Exec(deleteQuery, fileId)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("hosted file %s does not exist", fileId)
	}
	return nil
}

func (dbms *DBMS) DbHostedGet(fileId string) (*HostedFileData, error) {
	ok := dbms.DatabaseExists()
	if !ok {
		return nil, errors.New("database does not exist")
	}

	selectQuery := `SELECT FileId, Slug, FileName, LocalPath, FileSize, MimeType, Source, SourceMeta, Uploader, Downloads, Date FROM HostedFiles WHERE FileId = ?;`
	row := dbms.database.QueryRow(selectQuery, fileId)

	data := &HostedFileData{}
	err := row.Scan(&data.FileId, &data.Slug, &data.FileName, &data.LocalPath, &data.FileSize, &data.MimeType, &data.Source, &data.SourceMeta, &data.Uploader, &data.Downloads, &data.Date)
	if err != nil {
		return nil, fmt.Errorf("hosted file %s not found", fileId)
	}
	return data, nil
}

func (dbms *DBMS) DbHostedGetBySlug(slug string) (*HostedFileData, error) {
	ok := dbms.DatabaseExists()
	if !ok {
		return nil, errors.New("database does not exist")
	}

	selectQuery := `SELECT FileId, Slug, FileName, LocalPath, FileSize, MimeType, Source, SourceMeta, Uploader, Downloads, Date FROM HostedFiles WHERE Slug = ?;`
	row := dbms.database.QueryRow(selectQuery, slug)

	data := &HostedFileData{}
	err := row.Scan(&data.FileId, &data.Slug, &data.FileName, &data.LocalPath, &data.FileSize, &data.MimeType, &data.Source, &data.SourceMeta, &data.Uploader, &data.Downloads, &data.Date)
	if err != nil {
		return nil, fmt.Errorf("hosted file with slug %s not found", slug)
	}
	return data, nil
}

func (dbms *DBMS) DbHostedAll() []HostedFileData {
	var files []HostedFileData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT FileId, Slug, FileName, LocalPath, FileSize, MimeType, Source, SourceMeta, Uploader, Downloads, Date FROM HostedFiles ORDER BY Date;`
		query, err := dbms.database.Query(selectQuery)
		if err == nil {
			for query.Next() {
				data := HostedFileData{}
				err = query.Scan(&data.FileId, &data.Slug, &data.FileName, &data.LocalPath, &data.FileSize, &data.MimeType, &data.Source, &data.SourceMeta, &data.Uploader, &data.Downloads, &data.Date)
				if err != nil {
					continue
				}
				files = append(files, data)
			}
		} else {
			logs.Debug("", "Failed to query hosted files: "+err.Error())
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)
	}
	return files
}
