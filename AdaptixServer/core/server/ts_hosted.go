package server

import (
	"AdaptixServer/core/database"
	"AdaptixServer/core/utils/logs"
	"encoding/json"
	"errors"
	"fmt"
	"math/rand"
	"net/http"
	"os"
	"regexp"
	"sync/atomic"
	"time"
)

var slugRegex = regexp.MustCompile(`^[a-zA-Z0-9._\-/]+$`)

func (ts *Teamserver) hostedFileURL(slug string) string {
	return fmt.Sprintf("https://%s:%v%s/h/%s",
		ts.Profile.Server.Interface, ts.Profile.Server.Port, ts.Profile.Server.Endpoint, slug)
}

func (ts *Teamserver) registerHostedRoute(data database.HostedFileData) error {
	localPath := data.LocalPath
	fileName := data.FileName
	mimeType := data.MimeType
	fileId := data.FileId

	var downloadCount atomic.Int64
	downloadCount.Store(data.Downloads)

	handler := func(w http.ResponseWriter, r *http.Request) {
		newCount := downloadCount.Add(1)
		go ts.DBMS.DbHostedUpdateDownloads(fileId, newCount)

		w.Header().Set("Content-Type", mimeType)
		w.Header().Set("Content-Disposition", "attachment; filename="+fileName)
		http.ServeFile(w, r, localPath)
	}

	return ts.TsEndpointRegisterPublicRaw("GET", "/h/"+data.Slug, handler)
}

func (ts *Teamserver) unregisterHostedRoute(slug string) error {
	return ts.TsEndpointUnregisterPublic("GET", "/h/"+slug)
}

func (ts *Teamserver) TsHostedUpload(username string, slug string, fileName string, mimeType string, content []byte) (database.HostedFileData, error) {
	var data database.HostedFileData

	if slug == "" {
		slug = fileName
	}
	if mimeType == "" {
		mimeType = "application/octet-stream"
	}

	if !slugRegex.MatchString(slug) {
		return data, errors.New("invalid slug: only alphanumeric, dots, dashes, underscores, and forward slashes allowed")
	}

	// Check slug uniqueness
	existing, _ := ts.DBMS.DbHostedGetBySlug(slug)
	if existing != nil {
		return data, fmt.Errorf("slug '%s' is already in use", slug)
	}

	fileId := fmt.Sprintf("%08x", rand.Uint32())
	localPath := logs.RepoLogsInstance.HostedPath + "/" + fileId + "_" + fileName

	err := os.WriteFile(localPath, content, 0644)
	if err != nil {
		return data, fmt.Errorf("failed to write file: %s", err.Error())
	}

	data = database.HostedFileData{
		FileId:     fileId,
		Slug:       slug,
		FileName:   fileName,
		LocalPath:  localPath,
		FileSize:   int64(len(content)),
		MimeType:   mimeType,
		Source:     "upload",
		SourceMeta: "",
		Uploader:   username,
		Downloads:  0,
		Date:       time.Now().Unix(),
	}

	err = ts.DBMS.DbHostedInsert(data)
	if err != nil {
		_ = os.Remove(localPath)
		return data, err
	}

	err = ts.registerHostedRoute(data)
	if err != nil {
		_ = ts.DBMS.DbHostedDelete(fileId)
		_ = os.Remove(localPath)
		return data, fmt.Errorf("failed to register route: %s", err.Error())
	}

	url := ts.hostedFileURL(slug)
	packet := CreateSpHostedCreate(data, url)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryHostedRealtime)

	return data, nil
}

func (ts *Teamserver) TsHostedList() (string, error) {
	files := ts.DBMS.DbHostedAll()

	type HostedFileWithURL struct {
		database.HostedFileData
		URL string `json:"h_url"`
	}

	result := make([]HostedFileWithURL, 0, len(files))
	for _, f := range files {
		result = append(result, HostedFileWithURL{
			HostedFileData: f,
			URL:            ts.hostedFileURL(f.Slug),
		})
	}

	jsonData, err := json.Marshal(result)
	if err != nil {
		return "", err
	}
	return string(jsonData), nil
}

func (ts *Teamserver) TsHostedDelete(fileIds []string) error {
	var errs []string
	for _, fileId := range fileIds {
		data, err := ts.DBMS.DbHostedGet(fileId)
		if err != nil {
			errs = append(errs, err.Error())
			continue
		}

		_ = ts.unregisterHostedRoute(data.Slug)
		_ = os.Remove(data.LocalPath)

		err = ts.DBMS.DbHostedDelete(fileId)
		if err != nil {
			errs = append(errs, err.Error())
			continue
		}

		packet := CreateSpHostedDelete(fileId)
		ts.TsSyncAllClientsWithCategory(packet, SyncCategoryHostedRealtime)
	}

	if len(errs) > 0 {
		return fmt.Errorf("errors: %v", errs)
	}
	return nil
}

func (ts *Teamserver) TsHostedInit() {
	files := ts.DBMS.DbHostedAll()
	for _, f := range files {
		err := ts.registerHostedRoute(f)
		if err != nil {
			logs.Warn("", "Failed to register hosted route for %s: %s", f.Slug, err.Error())
		}
	}
	if len(files) > 0 {
		logs.Info("", "Registered %d hosted file(s)", len(files))
	}
}

func (ts *Teamserver) TsPresyncHosted() []interface{} {
	files := ts.DBMS.DbHostedAll()
	if len(files) == 0 {
		return nil
	}

	packets := make([]interface{}, 0, len(files))
	for _, f := range files {
		url := ts.hostedFileURL(f.Slug)
		p := CreateSpHostedCreate(f, url)
		packets = append(packets, p)
	}
	return packets
}
